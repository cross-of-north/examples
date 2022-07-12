<?

/*


*Общие принципы работы*


Для защиты от несанкционированных примитивных автоматизированных поисковых запросов выполняется проверка, работает ли на стороне клиента JavaScript.

1) При загрузке клиентом поисковой формы, в код основной страницы включается JS, выполняющий в контексте этой страницы JS, полученный через postMessage.
2) Кроме того, это скрипт создает невидимый iframe, загружающий с того же сайта JS, содержащий константы и алгоритм расчета хэша. Этот JS частично выполняется в контексте iframe, затем делает postMessage промежуточных результатов в основное окно.
3) Окончательное значение хэша считается в основном окне, и записывается в глобальную переменную window.form_guid .
4) Обработчик onsubmit формы, которая подлежит защите, при отсылке формы добавляет в нее скрытое поле, содержащее значение window.form_guid .
5) На сервере можно выполнить проверку присланного формой значения хэша с использованием сессии, созданной при загрузке формы поиска.
6) Если хэш не проходит проверку, отдельно на сервере можно проверить, соответствует ли хэш когда-либо создававшейся сессии.
7) Если хэш полностью некорректен, вместо запуска поиска клиенту в ответ высылается JS-редирект на корневую страницу сайта.

JS в пп. 1 и 2 обфусцирован (зашифрован/упакован), чтобы максимально затруднить создание корректного хэша без подключения полноценной интерпретации JS (даже если знать алгоритм вычисления, константы без расшифровки не вынешь).
Той же цели служит использование postMessage - это упрощенная проверка наличия на стороне клиента не просто выполнения JS, но и браузерного окружения (DOM).


*Структура хэша и алгоритм расчета*


Хэш состоит из двух конкатенированных частей H2 и H1 по 8 шестнадцатиричных разрядов (т.е. по 4 байта).

STATIC_SALT - константа в коде
RAND - случайное число, записываемое в сессию в начале работы, и во время жизни сессии изменяющееся только при смене IP.

USERAGENT - значение User-Agent, полученное сервером в HTTP-запросе, или значение navigator.userAgent на клиенте.
Для IE вырезаются дополнительные токены, т.к., начиная с версии IE9, они не посылаются на сервер.
Должен быть неизменным в течение сессии.

IP - IP-адрес клиента, видимый на сервере.

SALT = md5( PHPSESSID + IP + USERAGENT + STATIC_SALT + RAND )
SALT обеспечивает зависимость значения хэша от сессии, IP и браузера клиента.
Вследствие наличия RAND, не может быть посчитан на стороне клиента.

H1' = fnv1a( SALT + USERAGENT + PHPSESSID )
H1 = fnv1a( USERAGENT + H1' )
Расчет H1 является тем самым вычислением, выполнением которого клиент должен доказать, что он "настоящий".
На клиенте H1' рассчитывается внутри iframe, H1 - в основном окне.
Основным "секретным" параметром является SALT, уникальный для каждой сессии.

Z - номер дня в году.
H2' = fnv1a( USERAGENT + STATIC_SALT + Z + PHPSESSID )
H2 = fnv1a( H2' + H1 )
H2' не зависит от динамически меняющихся в зависимости от сессии значений (IP, SALT).
Преобразование H2' в H2 является маскировкой статичности этого значения.
Так, например, при передаче в Cookie старого PHPSESSID от истекшей сессии, значение H2' будет таким же, каким было для этой сессии исходно.
Но значение H2 не будет совпадать с исходным, т.к. в нем H2' смешан с RAND (через H1 и SALT), т.к. RAND для возобновленной сессии будет иным, несмотря на совпадение PHPSESSID.


*Логика работы сервера при проверке хэша*


Получение на стороне сервера значений H1 и H2 вместе с данными формы позволяет проверить:
a) Правильно ли посчитан на клиенте H1 для текущей сессии.
b) Могло ли для присланных H1, PHPSESSID, USERAGENT вчера или сегодня (варьируем Z) быть посчитано такое значение H2, при условии, что сессия PHPSESSID не существует.
c) Совпадает ли хэш с хэшем, посчитанным по IP последнего успешного визита в открытой ранее не истекшей сессии.

Варианты действий для различных комбинаций a, b, c:
- a0b0c0 - новый клиент, "не настоящий" клиент или клиент с истекшей давней сессией. Выполняется редирект в корень сайта, автоматически заново считается H1 и H2.
- a0b0c1 - клиент с активной сессией и изменившимся IP. Пересчитывается RAND и SALT, выполняется редирект в корень сайта, автоматически заново считается H1 и H2. Количество возможных смен IP в одной сессии ограничено; если оно превышено, то сессия испорчена, и клиент будет получать только a0b0c0 до истечения сессии.
- a0b1с* - клиент, прошедший проверку ранее, с истекшей недавней сессией. Данные формы от клиента можно принять и обработать. Одновременно с этим, необходимо обновить H1 на стороне клиента. В этом случае также можно действовать, как в случае a0b0с0.
- a1b*c* - клиент прошел проверку (b и c можно не проверять).

Технически возможные причины несовпадения хэша:
- Клиент не выполнил JS из-за ошибки или из-за того, что не может выполнять JS.
- В iframe не загрузился контент из-за ошибки JS, ошибки связи, или из-за того, что клиент не может создавать iframe.
- Клиент выполнил JS, но в основном окне или iframe navigator.userAgent не совпадает со значением видимым на сервере в User-Agent при загрузке формы.
- Вариант предыдущего случая, когда регулярное выражение, приводящее navigator.userAgent и User-Agent к единому виду, не смогло это сделать.
- У клиента изменился IP (мобильное устройство подключилось к другой сети).
- Истекла сессия на сервере.
- Клиент "придумал" PHPSESSID или получил его с другого сайта на том же домене.
- Клиент "придумал" хэш.


*Пример использования*

// ...
require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/debug/js_challenge.php");
// ...
if ( isset( $_POST['depart'] ) ) {
  IBEJavaScriptChallenge::CheckQuery();
}
// ...
require($_SERVER["DOCUMENT_ROOT"]."/bitrix/header.php");
// ...
echo IBEJavaScriptChallenge::Challenge();
// ...
require($_SERVER["DOCUMENT_ROOT"]."/bitrix/footer.php");

*Пример использования 2*

// ...
require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/debug/js_challenge.php");
// ...
require($_SERVER["DOCUMENT_ROOT"]."/bitrix/header.php");
// ...
if (
  (
    isset( $_POST[ "next_page" ] ) && $_POST[ "next_page" ] === "choose_trip"
    &&
    isset( $_POST[ "depart" ] )
    &&
    !isset( $_POST[ "USER_LOGIN" ] )
  )
  ||
  isset( $_POST['url_query_proc'] )
) {
  $hash = IBEJavaScriptChallenge::GetQueryHash();
  1||trace( array(
    "HASH" => $hash,
    "VALID_LIVE_SESSION" => IBEJavaScriptChallenge::IsValidLiveSession( $hash ),
    "VALID_STALE_SESSION" => IBEJavaScriptChallenge::IsValidStaleSession( $hash ),
    "VALID_CHANGED_IP_SESSION" => IBEJavaScriptChallenge::IsValidLiveSessionWithChangedIP( $hash ),
    "IP_CHANGE_COUNT" => IBEJavaScriptChallenge::GetIPChangeCount(),
  ) );
  $base_uri = isset( $_POST['url_query_proc'] ) ? $_POST['decode_url'] : $_SERVER[ "REQUEST_URI" ];
  $redirectOnErrorURL = reset( explode( "index.php", reset( explode( "deeplink", reset( explode( "!", reset( explode( "?", $base_uri ) ) ) ) ) ) ) );
  IBEJavaScriptChallenge::CheckQuery( array( "REDIRECT_ON_ERROR_URL" => $redirectOnErrorURL ) );
}
// ...
echo IBEJavaScriptChallenge::Challenge();
// ...
require($_SERVER["DOCUMENT_ROOT"]."/bitrix/footer.php");

*/


require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/debug/js_crypt.php");

class IBEJavaScriptChallenge {

static $static_salt = "jhbuyvi76"; // STATIC_SALT
static $bNewSession = false; // признак того, что сессия новая (не проходившая ранее через данный механизм)

static $default_options = array(
  "FIELD_NAME" => "guid",
  "SHOW_SCRIPT_TAG" => true,
  "CHALLENGE_LOAD_REPEAT_COUNT" => 5,
  "CHALLENGE_LOAD_REPEAT_INTERVAL" => 500,
  "CHALLENGE_LOAD_MAX_WAIT" => 10000,
  "REDIRECT_ON_ERROR_URL" => "",
  "CHECK_STALE" => true,
  "SITE_ROOT" => "",
  "USE_ENCRYPTION" => true,
);

// Регулярное выражение для обработки разницы User-Agent и navigator.userAgent в IE9+

// http://blogs.msdn.com/b/ie/archive/2010/03/23/introducing-ie9-s-user-agent-string.aspx
// We've received many reports  on compatibility issues due to long, extended UA strings.
// IE9 will send the short UA string detailed above without pre and post platform registry value tokens.
// This is interoperable with other browsers, and improves compatibility and network performance.
//
// Test:
/*
Mozilla/5.0 (Windows NT 6.1; WOW64; Trident/7.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; .NET4.0C; .NET4.0E; GWX:RESERVED; rv:11.0) like Gecko
Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/7.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; .NET4.0C; .NET4.0E; GWX:RESERVED)
Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; WOW64; Trident/7.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; .NET4.0C; .NET4.0E; GWX:RESERVED)
Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/7.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0; .NET4.0C; .NET4.0E; GWX:RESERVED)
Mozilla/5.0 (Windows NT 5.1; rv:33.0) Gecko/20100101 Firefox/33.0
*/
// to:
/*
Mozilla/5.0 (Windows NT 6.1; WOW64; Trident/7.0; rv:11.0) like Gecko
Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/7.0)
Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; WOW64; Trident/7.0)
Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/7.0)
Mozilla/5.0 (Windows NT 5.1; rv:33.0) Gecko/20100101 Firefox/33.0
*/

static $ua_replace_from = '/(?:(^.*Trident[^\s]*).*(\srv\:[^\s\;\)]*).*(\).*$))|(?:(^.*Trident[^\s\;]*).*(\).*$))|(.*)/';
static $ua_replace_to = '$1$2$3$4$5$6';

// п.1
// В Ajax-режиме запускается сразу, а не по onload
static function DoChallenge( $options ) {
  $options = array_merge( IBEJavaScriptChallenge::$default_options, (array)$options );
  @session_start();
  ob_start();
  ?>
  <?/**
  * Calculate a 32 bit FNV-1a hash
  * Found here: https://gist.github.com/vaiorabbit/5657561
  * Ref.: http://isthe.com/chongo/tech/comp/fnv/
  *
  * @param {string} str the input value
  * @param {boolean} [asString=false] set to true to return the hash value as
  *     8-digit hex string instead of an integer
  * @param {integer} [seed] optionally pass the hash of the previous chunk
  * @returns {integer | string}
  */
  ?>
  function hashFnv32a(str, asString, seed) {
      <?/*jshint bitwise:false */?>
      var i, l,
          hval = (seed === undefined) ? 0x811c9dc5 : seed;

      for (i = 0, l = str.length; i < l; i++) {
          hval ^= str.charCodeAt(i);
          hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
      }
      if( asString ){
          <?// Convert to 8 digit hex string?>
          return ("0000000" + (hval >>> 0).toString(16)).substr(-8);
      }
      return hval >>> 0;
  }
  (function(){
    var ifr = [];
    var max_count = <?= $options[ "CHALLENGE_LOAD_REPEAT_COUNT" ] ?>;
    function on_message( event ) {
      <?//console.log( navigator.userAgent );?>
      if ( event.origin === window.location.protocol+"//"+window.location.host && event.data && event.data.ibe_counter ) {
        eval( event.data.ibe_counter );
        if (document.addEventListener) {
          window.removeEventListener( "message", on_message );
        } else if (document.attachEvent) {
          window.detachEvent( "onmessage", on_message );
        }
        var i = 0;
        while ( ifr[i] ) {
          if ( ifr[i].parentNode ) {
            ifr[i].parentNode.removeChild(ifr[i]);
          }
          if ( ifr[i].timeouts !== undefined ) {
            delete window[ifr[i].name];
            var tc = 0;
            while ( ifr[i].timeouts[tc] ) {
              clearTimeout( ifr[i].timeouts[tc] );
              tc++;
            }
          }
          if ( ifr[i].cleanup !== undefined ) {
            eval( ifr[i].cleanup );
          }
          i++;
        }
      }
    }
    if (document.addEventListener) {
      window.addEventListener( "message", on_message, false );
    } else if (document.attachEvent) {
      window.attachEvent( "onmessage", on_message );
    }
    var k = 0;
    while ( k < max_count ) {
      ifr[k] = (document.attachEvent) && (/MSIE (6|7|8)/).test(navigator.userAgent) ?
          document.createElement('<iframe>'):
          document.createElement('iframe');
      ifr[k].style.display = "none";
      ifr[k].style.height = "1px";
      ifr[k].style.width = "1px";
      var v = parseInt( Math.random().toString().slice( 2, 15 ), 10 );
      ifr[k].src = "<?= $options[ "SITE_ROOT" ] ?>/bitrix/components/travelshop/ibe.counter/?"+( new Date().getTime() )+v+"&"+k;
      ifr[k].name = v;
      window[v] = ifr[k];
      window[v+1] = document.body;
      window[v+2] = document.body.appendChild;
      ifr[k].timeouts = [];
      <? /* cleanup function */ ?>
      ifr[k].cleanup = "var v;for(v=" + v + ";v<" + (v+4) + ";v++)delete window[v]";
      window[v+3] = "setTimeout(\""+ifr[k].cleanup+"\",1);";
      <? /* initiator will look like one-string VM without stack ( window['4396461560563'] ).call( window['4396461560562'], window['4396461560561'] );setTimeout(window['4396461560564'],0) */ ?>
      <? /* all window[] variables will be cleaned up */ ?>
      ifr[k].timeouts.push( setTimeout("window['" + v + "'].timeouts.push( setTimeout(\"( window['" + (v+2) + "'] ).call( window['" + (v+1) + "'], window['" + v + "'] );window['" + v + "'].timeouts.push( setTimeout(window['" + (v+3) + "'],0) )\",0) )", <?= $options[ "CHALLENGE_LOAD_REPEAT_INTERVAL" ] ?>*k) );
      k++;
    }
    <? /* response load failure processing */ ?>
    ifr[k] = {};
    ifr[k].timeouts = [];
    ifr[k].timeouts.push( setTimeout("window.form_guid='-'",<?= $options[ "CHALLENGE_LOAD_MAX_WAIT" ] - $options[ "CHALLENGE_LOAD_REPEAT_INTERVAL" ] * $options[ "CHALLENGE_LOAD_REPEAT_COUNT" ] ?>) );
  })();
  <?
  $s = ob_get_contents();
  ob_end_clean();
//  return $s;
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 3, // Упаковка JS-кода. Позволяет уменьшить объем исходного кода JavaScript (убрать лишние пробелы, урезать имена переменных) перед его криптованием. Вписывайте скрипт в поле ввода кода без тегов <script> и </script>.
    "cmode" => 1, // Уровень сжатия. Позволяет изменить уровень сжатия JavaScript-кода. Рекомендуемый уровень - Нормальное сжатие. 3 - Базовое, 0 - Минимальное, 1 - Нормальное.
    "cs" => 0, // Сжимать сложение строк. Если эта опция включена, то в скрипте будут удалены ненужные сложения строк, например 'abc'+'def' будет заменено на 'abcdef'.
    "fastdecode" => 0, // Быстрый распаковщик. Включите эту опцию, если необходим быстрый распаковщик кода. Отключите ее, если важен размер (в этом случае скрипт будет занимать на 100-200 байт меньше).
    ) );
  }
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 0, // Полиморфный код. Используйте эту опцию для наилучшего криптования кода/текста. Имеется полиморфный расшифровщик, полиморфный зашифрованный код, которые при каждом новом крипте меняются случайным образом.
    "change_sym_prob" => 30, // Вероятность смены ключа шифрования. Чем выше эта вероятность (100% максимум), тем чаще будет меняться ключ шифрования кода. Соответственно, тем больше будет и объем получаемого шифрованного кода.
    "divide_str_prob" => 30, // Вероятность разбиения кода. Чем выше эта вероятность (100% максимум), тем, больше раз криптованный код будет разбит на мелкие части. Соответственно, тем больше будет и объем получаемого шифрованного кода. Если эта вероятность больше нуля, тело расшифровщика также будет видоизменено.
    "split_func_prob" => 50, // Вероятность создания дополнительных функций. Чем выше эта вероятность (100% максимум), тем больше будет создано дополнительных функций, участвующих в распаковке кода. Размер кода будет больше, но и анализ его будет затруднен.
    "comp" => 1, // Полностью совместимый код. Если не включить эту опцию, то полученный код сможет работать только с кодировками windows-1251, ISO-8859-1, UTF-8, то есть где-то в начале (в <head>) страницы должен быть прописан тег <meta> с соответствующей кодировкой, например: <meta http-equiv="Content-Type" content="text/html;charset=windows-1251">. Если эта опция включена, то код будет совместим со всеми кодировками, но будет занимать больший объем.
    "objective" => 0, // Объектный код. При включении этой опции будет создан не функциональный, а объектный код. Включите эту опцию, если у Вас возникают конфликты JavaScript на странице из-за одинаковых имен переменных или функций. Используйте эту опцию, если необходимо вставить на одну страницу несколько криптованных кодов.
    "ceval" => 1, // Не использовать document.write. Включите эту опцию, если желаете, чтобы шифрованный код не использовал document.write. Это может быть необходимо при встраивании кода куда-либо еще, помимо браузера. В этом случае может быть успешно закриптован только JavaScript-код. Вписывайте его в поле ввода кода без тегов <script> и </script>.
    "attach" => 0, // Браузерная привязка. Позволяет задать, в каком именно браузере должен выполняться получаемый код. В других браузерах код выполнен не будет. 0 - Не создавать привязку, 1 - IE 6.0, 2 - IE 7.0, 3 - Firefox, 4 - Opera.
    ) );
  }
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 0, // Полиморфный код. Используйте эту опцию для наилучшего криптования кода/текста. Имеется полиморфный расшифровщик, полиморфный зашифрованный код, которые при каждом новом крипте меняются случайным образом.
    "change_sym_prob" => 0, // Вероятность смены ключа шифрования. Чем выше эта вероятность (100% максимум), тем чаще будет меняться ключ шифрования кода. Соответственно, тем больше будет и объем получаемого шифрованного кода.
    "divide_str_prob" => 0, // Вероятность разбиения кода. Чем выше эта вероятность (100% максимум), тем, больше раз криптованный код будет разбит на мелкие части. Соответственно, тем больше будет и объем получаемого шифрованного кода. Если эта вероятность больше нуля, тело расшифровщика также будет видоизменено.
    "split_func_prob" => 0, // Вероятность создания дополнительных функций. Чем выше эта вероятность (100% максимум), тем больше будет создано дополнительных функций, участвующих в распаковке кода. Размер кода будет больше, но и анализ его будет затруднен.
    "comp" => 1, // Полностью совместимый код. Если не включить эту опцию, то полученный код сможет работать только с кодировками windows-1251, ISO-8859-1, UTF-8, то есть где-то в начале (в <head>) страницы должен быть прописан тег <meta> с соответствующей кодировкой, например: <meta http-equiv="Content-Type" content="text/html;charset=windows-1251">. Если эта опция включена, то код будет совместим со всеми кодировками, но будет занимать больший объем.
    "objective" => 0, // Объектный код. При включении этой опции будет создан не функциональный, а объектный код. Включите эту опцию, если у Вас возникают конфликты JavaScript на странице из-за одинаковых имен переменных или функций. Используйте эту опцию, если необходимо вставить на одну страницу несколько криптованных кодов.
    "ceval" => 1, // Не использовать document.write. Включите эту опцию, если желаете, чтобы шифрованный код не использовал document.write. Это может быть необходимо при встраивании кода куда-либо еще, помимо браузера. В этом случае может быть успешно закриптован только JavaScript-код. Вписывайте его в поле ввода кода без тегов <script> и </script>.
    "attach" => 0, // Браузерная привязка. Позволяет задать, в каком именно браузере должен выполняться получаемый код. В других браузерах код выполнен не будет. 0 - Не создавать привязку, 1 - IE 6.0, 2 - IE 7.0, 3 - Firefox, 4 - Opera.
    ) );
  }
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 3, // Упаковка JS-кода. Позволяет уменьшить объем исходного кода JavaScript (убрать лишние пробелы, урезать имена переменных) перед его криптованием. Вписывайте скрипт в поле ввода кода без тегов <script> и </script>.
    "cmode" => 1, // Уровень сжатия. Позволяет изменить уровень сжатия JavaScript-кода. Рекомендуемый уровень - Нормальное сжатие. 3 - Базовое, 0 - Минимальное, 1 - Нормальное.
    "cs" => 0, // Сжимать сложение строк. Если эта опция включена, то в скрипте будут удалены ненужные сложения строк, например 'abc'+'def' будет заменено на 'abcdef'.
    "fastdecode" => 0, // Быстрый распаковщик. Включите эту опцию, если необходим быстрый распаковщик кода. Отключите ее, если важен размер (в этом случае скрипт будет занимать на 100-200 байт меньше).
    ) );
  }

  if ( $options[ "SHOW_SCRIPT_TAG" ] ) {
    $s = IBEJavaScriptChallenge::HideFromAV( $s );
  }

  return $s;
}

// п.2
static function Response( $options ) {
  $options = array_merge( IBEJavaScriptChallenge::$default_options, (array)$options );
  @session_start();
  $salt = IBEJavaScriptChallenge::GetSalt( IBEJavaScriptChallenge::GetClientIP() );

  ob_start();
  ?>
  <?/**
  * Calculate a 32 bit FNV-1a hash
  * Found here: https://gist.github.com/vaiorabbit/5657561
  * Ref.: http://isthe.com/chongo/tech/comp/fnv/
  *
  * @param {string} str the input value
  * @param {boolean} [asString=false] set to true to return the hash value as
  *     8-digit hex string instead of an integer
  * @param {integer} [seed] optionally pass the hash of the previous chunk
  * @returns {integer | string}
  */?>
  function hashFnv32a(str, asString, seed) {
      <?/*jshint bitwise:false */?>
      var i, l,
          hval = (seed === undefined) ? 0x811c9dc5 : seed;

      for (i = 0, l = str.length; i < l; i++) {
          hval ^= str.charCodeAt(i);
          hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
      }
      if( asString ){
          <?// Convert to 8 digit hex string?>
          return ("0000000" + (hval >>> 0).toString(16)).substr(-8);
      }
      return hval >>> 0;
  }
  <?
    // str_replace( "\\", "\\\\\\", ... ) - в тех случаях, когда регулярное выражение - это часть JS-строки, передаваемой в eval()
  ?>
  if ( window.parent ) {
    <?//console.log( navigator.userAgent );?>
    <?/* //console.log( navigator.userAgent.replace(<?= IBEJavaScriptChallenge::$ua_replace_from ?>, '<?= IBEJavaScriptChallenge::$ua_replace_to ?>') ); */?>
    window.parent.postMessage({"ibe_counter":
      <? // H1 = fnv1a( USERAGENT + H1' ) ?>
      "var hash=hashFnv32a(" +
      "navigator.userAgent.replace(<?= str_replace( "\\", "\\\\\\", IBEJavaScriptChallenge::$ua_replace_from ) ?>, '<?= IBEJavaScriptChallenge::$ua_replace_to ?>')" +
      "+'" +
      <? // H1' = fnv1a( SALT + USERAGENT + PHPSESSID ) ?>
      hashFnv32a(
        '<?= $salt ?>' +
        navigator.userAgent.replace(<?= IBEJavaScriptChallenge::$ua_replace_from ?>, '<?= IBEJavaScriptChallenge::$ua_replace_to ?>') +
        '<?= session_id() ?>'
      ,true) +
      "',true);" +
      <? // H2 = fnv1a( H2' + H1 ) ?>
      "window.form_guid=hashFnv32a('<?= IBEJavaScriptChallenge::GetStaleSessionHash() ?>'+hash,true)" +
      <? // H2.H1 ?>
      "+hash;"
      <?/* //+ "console.log( navigator.userAgent.replace(<?= str_replace( "\\", "\\\\\\", IBEJavaScriptChallenge::$ua_replace_from ) ?>, '<?= IBEJavaScriptChallenge::$ua_replace_to ?>') );" */?>
    },window.location.protocol+"//"+window.location.host+"/");
  }
  <?
  $s = ob_get_contents();
  ob_end_clean();
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 3, // Упаковка JS-кода. Позволяет уменьшить объем исходного кода JavaScript (убрать лишние пробелы, урезать имена переменных) перед его криптованием. Вписывайте скрипт в поле ввода кода без тегов <script> и </script>.
    "cmode" => 1, // Уровень сжатия. Позволяет изменить уровень сжатия JavaScript-кода. Рекомендуемый уровень - Нормальное сжатие. 3 - Базовое, 0 - Минимальное, 1 - Нормальное.
    "cs" => 0, // Сжимать сложение строк. Если эта опция включена, то в скрипте будут удалены ненужные сложения строк, например 'abc'+'def' будет заменено на 'abcdef'.
    "fastdecode" => 0, // Быстрый распаковщик. Включите эту опцию, если необходим быстрый распаковщик кода. Отключите ее, если важен размер (в этом случае скрипт будет занимать на 100-200 байт меньше).
    ) );
  }
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 0, // Полиморфный код. Используйте эту опцию для наилучшего криптования кода/текста. Имеется полиморфный расшифровщик, полиморфный зашифрованный код, которые при каждом новом крипте меняются случайным образом.
    "change_sym_prob" => 30, // Вероятность смены ключа шифрования. Чем выше эта вероятность (100% максимум), тем чаще будет меняться ключ шифрования кода. Соответственно, тем больше будет и объем получаемого шифрованного кода.
    "divide_str_prob" => 30, // Вероятность разбиения кода. Чем выше эта вероятность (100% максимум), тем, больше раз криптованный код будет разбит на мелкие части. Соответственно, тем больше будет и объем получаемого шифрованного кода. Если эта вероятность больше нуля, тело расшифровщика также будет видоизменено.
    "split_func_prob" => 50, // Вероятность создания дополнительных функций. Чем выше эта вероятность (100% максимум), тем больше будет создано дополнительных функций, участвующих в распаковке кода. Размер кода будет больше, но и анализ его будет затруднен.
    "comp" => 1, // Полностью совместимый код. Если не включить эту опцию, то полученный код сможет работать только с кодировками windows-1251, ISO-8859-1, UTF-8, то есть где-то в начале (в <head>) страницы должен быть прописан тег <meta> с соответствующей кодировкой, например: <meta http-equiv="Content-Type" content="text/html;charset=windows-1251">. Если эта опция включена, то код будет совместим со всеми кодировками, но будет занимать больший объем.
    "objective" => 0, // Объектный код. При включении этой опции будет создан не функциональный, а объектный код. Включите эту опцию, если у Вас возникают конфликты JavaScript на странице из-за одинаковых имен переменных или функций. Используйте эту опцию, если необходимо вставить на одну страницу несколько криптованных кодов.
    "ceval" => 1, // Не использовать document.write. Включите эту опцию, если желаете, чтобы шифрованный код не использовал document.write. Это может быть необходимо при встраивании кода куда-либо еще, помимо браузера. В этом случае может быть успешно закриптован только JavaScript-код. Вписывайте его в поле ввода кода без тегов <script> и </script>.
    "attach" => 0, // Браузерная привязка. Позволяет задать, в каком именно браузере должен выполняться получаемый код. В других браузерах код выполнен не будет. 0 - Не создавать привязку, 1 - IE 6.0, 2 - IE 7.0, 3 - Firefox, 4 - Opera.
    ) );
  }
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 0, // Полиморфный код. Используйте эту опцию для наилучшего криптования кода/текста. Имеется полиморфный расшифровщик, полиморфный зашифрованный код, которые при каждом новом крипте меняются случайным образом.
    "change_sym_prob" => 0, // Вероятность смены ключа шифрования. Чем выше эта вероятность (100% максимум), тем чаще будет меняться ключ шифрования кода. Соответственно, тем больше будет и объем получаемого шифрованного кода.
    "divide_str_prob" => 0, // Вероятность разбиения кода. Чем выше эта вероятность (100% максимум), тем, больше раз криптованный код будет разбит на мелкие части. Соответственно, тем больше будет и объем получаемого шифрованного кода. Если эта вероятность больше нуля, тело расшифровщика также будет видоизменено.
    "split_func_prob" => 0, // Вероятность создания дополнительных функций. Чем выше эта вероятность (100% максимум), тем больше будет создано дополнительных функций, участвующих в распаковке кода. Размер кода будет больше, но и анализ его будет затруднен.
    "comp" => 1, // Полностью совместимый код. Если не включить эту опцию, то полученный код сможет работать только с кодировками windows-1251, ISO-8859-1, UTF-8, то есть где-то в начале (в <head>) страницы должен быть прописан тег <meta> с соответствующей кодировкой, например: <meta http-equiv="Content-Type" content="text/html;charset=windows-1251">. Если эта опция включена, то код будет совместим со всеми кодировками, но будет занимать больший объем.
    "objective" => 0, // Объектный код. При включении этой опции будет создан не функциональный, а объектный код. Включите эту опцию, если у Вас возникают конфликты JavaScript на странице из-за одинаковых имен переменных или функций. Используйте эту опцию, если необходимо вставить на одну страницу несколько криптованных кодов.
    "ceval" => 1, // Не использовать document.write. Включите эту опцию, если желаете, чтобы шифрованный код не использовал document.write. Это может быть необходимо при встраивании кода куда-либо еще, помимо браузера. В этом случае может быть успешно закриптован только JavaScript-код. Вписывайте его в поле ввода кода без тегов <script> и </script>.
    "attach" => 0, // Браузерная привязка. Позволяет задать, в каком именно браузере должен выполняться получаемый код. В других браузерах код выполнен не будет. 0 - Не создавать привязку, 1 - IE 6.0, 2 - IE 7.0, 3 - Firefox, 4 - Opera.
    ) );
  }
  if ( $options[ "USE_ENCRYPTION" ] ) {
    $s = JavaScriptCryptor::crypt( $s, array(
    "crypttype" => 3, // Упаковка JS-кода. Позволяет уменьшить объем исходного кода JavaScript (убрать лишние пробелы, урезать имена переменных) перед его криптованием. Вписывайте скрипт в поле ввода кода без тегов <script> и </script>.
    "cmode" => 1, // Уровень сжатия. Позволяет изменить уровень сжатия JavaScript-кода. Рекомендуемый уровень - Нормальное сжатие. 3 - Базовое, 0 - Минимальное, 1 - Нормальное.
    "cs" => 0, // Сжимать сложение строк. Если эта опция включена, то в скрипте будут удалены ненужные сложения строк, например 'abc'+'def' будет заменено на 'abcdef'.
    "fastdecode" => 0, // Быстрый распаковщик. Включите эту опцию, если необходим быстрый распаковщик кода. Отключите ее, если важен размер (в этом случае скрипт будет занимать на 100-200 байт меньше).
    ) );
  }
  if ( $options[ "SHOW_SCRIPT_TAG" ] ) {
    $s = IBEJavaScriptChallenge::HideFromAV( $s, $bShowHTMLTag = true );
  }
  return $s;
}

static function fnv1a($s)
{
  $hash = 2166136261;
  foreach (str_split($s) as $chr)
  {
    $hash = $hash ^ ord($chr);
    $hash = $hash & 0x0ffffffff;
    $hash += ($hash << 1) + ($hash << 4) + ($hash << 7) + ($hash << 8) + ($hash << 24);
    $hash = $hash & 0x0ffffffff;
  }
  $hash = $hash & 0x0ffffffff;
  return sprintf('%08s', dechex( $hash ) );
}

static function GetUserAgent() {
  return isset($_SERVER[ "HTTP_USER_AGENT" ]) ? preg_replace( IBEJavaScriptChallenge::$ua_replace_from, IBEJavaScriptChallenge::$ua_replace_to, $_SERVER[ "HTTP_USER_AGENT" ] ) : "";
}

// SALT
static function GetSalt( $ip ) {
  @session_start();
  if ( !isset( $_SESSION[ "IBEJavaScriptChallenge_salt" ] ) ) {
    IBEJavaScriptChallenge::$bNewSession = true;
    $_SESSION[ "IBEJavaScriptChallenge_salt" ] = rand(); // RAND
    $_SESSION[ "IBEJavaScriptChallenge_IP" ] = $ip;
  }
  if ( !isset( $_SESSION[ "IBEJavaScriptChallenge_IP_change_count" ] ) ) {
    $_SESSION[ "IBEJavaScriptChallenge_IP_change_count" ] = 0;
  }
  // SALT = md5( PHPSESSID + IP + USERAGENT + STATIC_SALT + RAND )
  return md5( session_id().$ip.IBEJavaScriptChallenge::GetUserAgent().IBEJavaScriptChallenge::$static_salt.$_SESSION[ "IBEJavaScriptChallenge_salt" ] );
}

// H1
static function GetLiveSessionHash( $ip ) {
  @session_start();
  $salt = IBEJavaScriptChallenge::GetSalt( $ip );
  // H1' = fnv1a( SALT + USERAGENT + PHPSESSID )
  // H1 = fnv1a( USERAGENT + H1' )
  return IBEJavaScriptChallenge::fnv1a( IBEJavaScriptChallenge::GetUserAgent()."".IBEJavaScriptChallenge::fnv1a( $salt.IBEJavaScriptChallenge::GetUserAgent().session_id() ) );
}

// H2'
static function GetStaleSessionHash( $day_offset = 0 ) {
  @session_start();
  // H2' = fnv1a( USERAGENT + STATIC_SALT + Z + PHPSESSID )
  return IBEJavaScriptChallenge::fnv1a( IBEJavaScriptChallenge::GetUserAgent().IBEJavaScriptChallenge::$static_salt.( date( "z" ) + $day_offset ).session_id() );
}

static function IsNewSession() {
  @session_start();
  return !isset( $_SESSION[ "IBEJavaScriptChallenge_salt" ] ) || IBEJavaScriptChallenge::$bNewSession;
}

static function DoIsValidLiveSession( $s, $ip ) {
  @session_start();
  $bLiveSessionValid = ( substr( $s, 8 ) == IBEJavaScriptChallenge::GetLiveSessionHash( $ip ) );
  return $bLiveSessionValid;
}

static function GetClientIP() {
  return isset($_SERVER['REMOTE_ADDR'])?$_SERVER['REMOTE_ADDR']:"";
}

// a
static function IsValidLiveSession( $s ) {
  @session_start();
  return IBEJavaScriptChallenge::DoIsValidLiveSession( $s, IBEJavaScriptChallenge::GetClientIP() );
}

// c
static function IsValidLiveSessionWithChangedIP( $s ) {
  @session_start();
  return (
    !IBEJavaScriptChallenge::IsValidLiveSession( $s )
    &&
    !IBEJavaScriptChallenge::IsNewSession()
    &&
    isset( $_SESSION[ "IBEJavaScriptChallenge_IP" ] )
    &&
    IBEJavaScriptChallenge::DoIsValidLiveSession( $s, $_SESSION[ "IBEJavaScriptChallenge_IP" ] )
  );
   {
    $_SESSION[ "IBEJavaScriptChallenge_IP_change_count" ]++;
    unset( $_SESSION[ "IBEJavaScriptChallenge_salt" ] );
  }
  return $bLiveSessionValid;
}

static function GetIPChangeCount() {
  @session_start();
  return isset( $_SESSION[ "IBEJavaScriptChallenge_IP_change_count" ] ) ? $_SESSION[ "IBEJavaScriptChallenge_IP_change_count" ] : 0;
}

static function RegisterIPChange() {
  @session_start();
  $_SESSION[ "IBEJavaScriptChallenge_IP_change_count" ]++;
  unset( $_SESSION[ "IBEJavaScriptChallenge_salt" ] );
  unset( $_SESSION[ "IBEJavaScriptChallenge_IP" ] );
}

// b
static function IsValidStaleSession( $s ) {
  @session_start();
  $bStaleSession = false;
  if ( IBEJavaScriptChallenge::IsNewSession() ) {
    $stale_session_hash = substr( $s, 0, 8 );
    $live_session_hash = substr( $s, 8 );
    $bStaleSession = (
      $stale_session_hash == IBEJavaScriptChallenge::fnv1a( IBEJavaScriptChallenge::GetStaleSessionHash().$live_session_hash )
      ||
      $stale_session_hash == IBEJavaScriptChallenge::fnv1a( IBEJavaScriptChallenge::GetStaleSessionHash( -1 ).$live_session_hash )
    );
  }
  return $bStaleSession;
}

static function GetQueryHash( $field_name = null ) {
  @session_start();
  if ( $field_name === null || ( $field_name !== null && strlen( $field_name ) == 0 ) ) {
    $field_name = IBEJavaScriptChallenge::$default_options;
  }
  if ( is_array( $field_name ) ) {
    $field_name = $field_name[ "FIELD_NAME" ];
  }
  return ( isset( $_POST[ $field_name ] ) ? $_POST[ $field_name ] : ( isset( $_GET[ $field_name ] ) ? $_GET[ $field_name ] : false ) );
}

// Варианты действий для различных комбинаций a, b, c
static function CheckQuery( $options ) {
  $options = array_merge( IBEJavaScriptChallenge::$default_options, (array)$options );
  @session_start();
  $hash = IBEJavaScriptChallenge::GetQueryHash( $options );
  if (
    $hash === false
    ||
    (
      !IBEJavaScriptChallenge::IsValidLiveSession( $hash )
      &&
      !(
        $options[ "CHECK_STALE" ]
        &&
        IBEJavaScriptChallenge::IsValidStaleSession( $hash )
      )
    )
  ) {
    if ( IBEJavaScriptChallenge::IsValidLiveSessionWithChangedIP( $hash ) ) {
      if ( IBEJavaScriptChallenge::GetIPChangeCount() < 4 ) {
        IBEJavaScriptChallenge::RegisterIPChange();
      }
    }
    ?><script>window.location='<?= $options[ "REDIRECT_ON_ERROR_URL" ] ?>';</script><?
    die();
  }
}

// п.1 для Ajax
static function RechallengeStaleAjaxSession( $options ) {
  $options = array_merge( IBEJavaScriptChallenge::$default_options, array( "SHOW_SCRIPT_TAG" => false, "AJAX" => true ), (array)$options );
  @session_start();
  require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/ibe/utf8.php");
  require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/ibe/ibe_ajax.php");
  $hash = IBEJavaScriptChallenge::GetQueryHash( $options );
  if ( $hash !== false && CIBEAjax::IsAjaxMode() && IBEJavaScriptChallenge::IsValidStaleSession( $hash ) ) {
    CIBEAjax::AddEval( IBEJavaScriptChallenge::DoChallenge( $options ) );
  }
}

// п.1 для скриптов без подключения фреймворка (ibe.deeplink)
static function ChallengeRaw( $options ) {
  $options = array_merge( IBEJavaScriptChallenge::$default_options, (array)$options );
  @session_start();
  return IBEJavaScriptChallenge::DoChallenge( $options );
}

// п.1
static function Challenge( $options ) {
  $options = array_merge( IBEJavaScriptChallenge::$default_options, (array)$options );
  @session_start();
  require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/ibe/utf8.php");
  require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/ibe/ibe_ajax.php");
  if ( CIBEAjax::IsAjaxMode() ) {
    IBEJavaScriptChallenge::RechallengeStaleAjaxSession( $options );
    return "";
  } else {
    return IBEJavaScriptChallenge::DoChallenge( $options );
  }
}

// Следующая обертка нужна для того, чтобы антивирусы (в частности, AVAST) не находили контент подозрительным.
// Предположительно, помещение скрипта в <style> позволяет обойти статический анализ, а запуск по onclick - динамический.
/*
ALYac JS:Exploit.BlackHole.ST 20151111
Arcabit JS:Exploit.BlackHole.ST 20151111
Avast JS:Redirector-UC [Trj] 20151111
BitDefender JS:Exploit.BlackHole.ST 20151111
Emsisoft JS:Exploit.BlackHole.ST (B) 20151111
F-Secure JS:Exploit.BlackHole.ST 20151111
GData JS:Exploit.BlackHole.ST 20151111
MicroWorld-eScan JS:Exploit.BlackHole.ST 20151111
Microsoft Trojan:JS/Redirector.JN 20151111
NANO-Antivirus Trojan.Script.Agent.rrcam 20151111
nProtect JS:Exploit.BlackHole.ST 20151111

+ Qihoo-360 htm.shellcode.am.gen 20151111
На VirusTotal ругается на TravelShop всегда, даже без IBEJavaScriptChallenge.
"Вживую" сайт не блокирует.
*/
static function HideFromAV( $s, $bShowHTMLTag = false ) {
  ob_start();
  if ( $bShowHTMLTag ) {
  ?><!DOCTYPE html>
<html>
<head>
</head>
<body><?
  }
  ?>
  <style id="ibe_counter_css">/*<![CDATA[
  <?= $s ?>
  ]]*/></style>
  <span id="ibe_counter_wrapper" style="display:none" onclick='
    var s=document.getElementById("ibe_counter_css").innerHTML;
    eval(s.substring(11,s.length-5));
    setTimeout("var obj=document.getElementById( \"ibe_counter_wrapper\" );obj.parentNode.removeChild(obj);obj=document.getElementById( \"ibe_counter_css\" );obj.parentNode.removeChild(obj);obj=document.getElementById( \"ibe_counter_loader\" );obj.parentNode.removeChild(obj)",0);
  '></span>
  <script type="text/javascript" id="ibe_counter_loader">
  // <![CDATA[
  (function(){
    function bootstrap() {
      if ( document.getElementById( "ibe_counter_wrapper" ).click === undefined && document.dispatchEvent ) {
        <? /* safari */ ?>
        document.getElementById( "ibe_counter_wrapper" ).click = function() {
          var o = document.createEvent('MouseEvents');
          o.initMouseEvent('click', true, true, window, 1, 1, 1, 1, 1, false, false, false, false, 0, this);
          this.dispatchEvent(o);
        };
      }
      setTimeout( 'document.getElementById( "ibe_counter_wrapper" ).click()', 0 );
    }
    <? /*bootstrap();*/ ?>
    if (window.addEventListener) {
      window.addEventListener('load', bootstrap);
    } else if (window.attachEvent) {
      window.attachEvent('onload', bootstrap);
    }
  })();
  // ]]>
  </script>
  <?
  if ( $bShowHTMLTag ) {
  ?></body>
</html><?
  }
  $s = ob_get_contents();
  ob_end_clean();

  return $s;
}

} // class IBEJavaScriptChallenge
