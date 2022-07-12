
const config = require("twd_config");
const dm2 = require("twd_dm2");

const CPP_CUTENAME = "JS CUTE module";
const CUPPS_CUTENAME = "CUPPS";

var full_settings_set = {};

function get_settings_by_device_name(name) {
    var section = false;
    for (var section_name in full_settings_set) {
        var section_from_ini = full_settings_set[section_name];
        if (section_from_ini.devname === name) {
            section = section_from_ini;
            break;
        }
    }
    return section;
}

function is_js_cute_name(s) {
    return s === CUPPS_CUTENAME;
}

function before_callback(callback_set_by, msg) {
    var bResult = true;
    console.log("cfg proxy before " + callback_set_by + " callback", msg);
    switch (callback_set_by) {
        case "getDevicesFromReginaToDM": {
            // передача информации из twd_config (как есть в ini-файле) в twd_dm
            full_settings_set = {};
            msg.forEach && msg.forEach(value => {
                var o = {};
                for (var item_name in value) {
                    o[item_name] = value[item_name];
                }
                full_settings_set[value.section] = o;
                if (is_js_cute_name(value.cutename)) {
                    value.js_cutename = value.cutename;
                    value.cutename = CPP_CUTENAME;
                }
            });
            console.log(full_settings_set);
            break;
        }
        case "getDevicesFromReginaToJS": {
            // передача информации из twd_config (как есть в ini-файле отфильтрованное по тому, как twd_config представляет визуальный редактор) в визуальный редактор
            // не известные twd_config параметры отсутствуют, не известные значения параметров искажены
            msg.forEach && msg.forEach(device => { // tab
                var section_from_ini = false;
                device.Groups && device.Groups.forEach && device.Groups.forEach(group => { // section
                    group.Settings && group.Settings.forEach && group.Settings.forEach(setting => { // setting
                        if (device.name && setting.name === "cutename" && setting.options && setting.options instanceof Array) {
                            setting.options = setting.options.filter(item => item !== CPP_CUTENAME);
                            setting.options.push(CUPPS_CUTENAME);
                            //console.log(full_settings_set);
                            section_from_ini = get_settings_by_device_name(device.name);
                            //console.log("==>",section_from_ini, device);
                            if (section_from_ini) {
                                setting.default = section_from_ini.cutename;
                            }
                            device.cutename = setting.default;
                        }
                    });
                });
                if (section_from_ini) {
                    device.section_from_ini = section_from_ini;
                }
            });
            dm2.preprocess_settings(msg);
            break;
        }
    }
    return bResult;
}

function after_callback(callback_set_by, msg, result) {
    //console.log("cfg proxy after " + callback_set_by + " callback", msg, result);
}

const handler = {
    get(target, propKey, receiver) {
        const origMethod = target[propKey];
        return function (...args) {
            var original_callback_function_index = -1;
            switch (propKey) {
                case "getTapsFromRegina": {
                    original_callback_function_index = 0;
                    break;
                }
                case "getDevicesFromReginaToDM": {
                    // args[1] - PATCH_GET_DEVICES
                    if (args[1] === undefined || args[1]) {
                        original_callback_function_index = 0;
                    }
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
                case "onClearCachedConfigAsync": {
                    original_callback_function_index = 0;
                    break;
                }
                case "getDevicesFromReginaToJS": {

                    // параметры передаются из редактора, чтобы получить набор настроек другой CUTE-системы, не той, что в настройках
                    // для наших целей это равносильно сохранению изменного названия CUTE-системы (чтобы в callback выдать настройки уже для нового названия)
                    var force_cutename = args[0];
                    var force_firmware = args[1];
                    //console.log(full_settings_set);
                    for (var section_name in full_settings_set) {
                        var section_from_ini = full_settings_set[section_name];
                        if (force_cutename[section_from_ini.devname]) {
                            section_from_ini.cutename = force_cutename[section_from_ini.devname];
                        }
                        if (force_firmware[section_from_ini.devname]) {
                            section_from_ini.firmware = force_firmware[section_from_ini.devname];
                        }
                        // даже если на входе параметры не заполнены, мы хотим их посылать, чтобы для JS CUTE получить именно параметры JS CUTE
                        force_cutename[section_from_ini.devname] = section_from_ini.cutename;
                        if (is_js_cute_name(section_from_ini.cutename)) {
                            force_cutename[section_from_ini.devname] = CPP_CUTENAME;
                        }
                    }
                    //console.log(full_settings_set);

                    original_callback_function_index = 2;
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
            let result = origMethod.apply(this, args);
            //console.log("cfg proxy: " + propKey, args, result);
            return result;
        };
    }
};

module.exports = new Proxy( config, handler );
