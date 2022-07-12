
const dm = require("twd_dm");

const CUPPS_CUTENAME = "CUPPS";

const supported_cute_list = {};
supported_cute_list[CUPPS_CUTENAME] = require("twd_cupps");

var devices = {};

function get_device_by_name(name) {
    var device = false;
    for (var device_handle in devices) {
        var item = devices[device_handle];
        if (item.name === name) {
            device = item;
            break;
        }
    }
    return device;
}

function before_callback(callback_set_by, msg) {
    var bResult = true;
    //console.log("dm2 proxy before " + callback_set_by + " callback " + msg.devname + ":" + msg.type, msg);
    if ((callback_set_by === "setDevices" || callback_set_by === "setWatcher") && msg.type === "jscomm_message") {
        //console.log("dm2 proxy jscomm_message: " + msg.devname + " #" + msg.data.handle + " " + msg.data.command, msg.data);
        if (msg.data.command === "create") {
            var device = supported_cute_list[msg.data.params.js_cutename].create_device(msg);
            device.name || (device.name = msg.devname);
            devices[msg.data.handle] = device;
        }
        if (devices[msg.data.handle]) {
            switch (msg.data.command) {
                case "close":
                case "read":
                case "write":
                case "change_focus":
                case "check_ready": {
                    if (!devices[msg.data.handle].bOpen) {
                        console.log("dm2: " + msg.devname + " #" + msg.data.handle + " not opened");
                        setTimeout(() => {
                            dm.onJSCommMessageProcessed({
                                handle: msg.data.handle,
                                result: false
                            });
                        }, 0);
                        break;
                    } else {
                        // fall through
                    }
                }
                default: {
                    devices[msg.data.handle][msg.data.command](msg, (msg_out) => {
                        if (msg_out.result === true) {
                            if (msg.data.command == "open") {
                                devices[msg.data.handle].bOpen = true;
                            } else if (msg.data.command == "close") {
                                devices[msg.data.handle].bOpen = false;
                            }
                        }
                        msg_out.handle = msg.data.handle;
                        dm.onJSCommMessageProcessed(msg_out);
                    });
                }
            }
        } else {
            console.log("dm2: orphan message", msg);
        }
        if (msg.data.command === "destroy") {
            delete devices[msg.data.handle];
        }
        bResult = false;
    }
    return bResult;
}

function after_callback(callback_set_by, msg, result) {
    //console.log("dm2 proxy after " + callback_set_by + " callback", msg, result);
}

function test_buffer_to_string(buffer) {
    var data = new Buffer(buffer.replace("data:application/octet-stream;base64,", "").replace("&#x3D;", "="), 'base64').toString('ascii');
    data = data.replace(/\\0x([A-Fa-f0-9]{2})/g, function (_, match) {
        return String.fromCharCode(parseInt(match, 16));
    });
    return data;
}

function string_to_array_buffer(s) {
    var data = new Buffer(s);
    data = data.buffer.slice(data.byteOffset, data.byteLength + data.byteOffset);
    return data;
}

const handler = {
    get(target, propKey, receiver) {
        var origMethod = target[propKey];
        return function (...args) {
            var original_callback_function_index = -1;
            switch (propKey) {
                case "setDevices": {
                    //console.log("dm2 proxy setDevices", args);
                    original_callback_function_index = 1;
                    break;
                }
                case "setWatcher": {
                    original_callback_function_index = 0;
                    break;
                }
                case "onTerminate": {
                    original_callback_function_index = 0;
                    break;
                }
                case "get": {
                    break;
                }
                case "set": {
                    break;
                }
                case "getAll": {
                    break;
                }
                case "messageToDM": {
                    if (args[0].type === "command_test") {
                        var device = get_device_by_name(args[0].devname);
                        if (device) {
                            var data = test_buffer_to_string(args[0].data);
                            if (data.substr(0, 13).toLowerCase() === "emulnewstatus") {
                                data = data.substr(13);
                                var status = parseInt(data, 16);
                                device.test_reading(false, status);
                                origMethod = false;
                            } else if (args[0].devname === "ocr") {
                                data = string_to_array_buffer(data);
                                device.test_reading(data);
                                origMethod = false;
                            } else if (args[0].devname === "bgr") {
                                if (data.substr(0, 4).toLowerCase() === "data") {
                                    data = string_to_array_buffer(data.substr(4));
                                    device.test_reading(data);
                                    origMethod = false;
                                }
                            }
                        }
                    }
                    break;
                }
            }
            if (original_callback_function_index != -1) {
                var original_function = args[original_callback_function_index];
                if (original_function && original_function.callback) {
                    original_function = original_function.callback;
                }
                args[original_callback_function_index] = function (...callback_args) {
                    let call_parent = before_callback(propKey, callback_args[0]);
                    let result = undefined;
                    if (call_parent && original_function) {
                        result = original_function.apply(this, callback_args);
                    }
                    after_callback(propKey, callback_args[0], result)
                    //console.log("proxy setDevices callback" + (original_function ? "" : " [null]"), callback_args, result);
                    return result;
                }
            }
            let result = origMethod ? origMethod.apply(this, args) : true;
            //console.log("dm proxy: " + propKey, args, result);
            return result;
        };
    }
};

module.exports = new Proxy( dm, handler );

module.exports.preprocess_settings = msg => {
    msg.forEach && msg.forEach(device => {
        if (supported_cute_list[device.cutename]) {
            supported_cute_list[device.cutename].preprocess_settings(device);
        }
    });
};