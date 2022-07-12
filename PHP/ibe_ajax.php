<?
require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/ibe/ibe_detailed_error.php");

class CIBEAjax {

  static $current_area;
  private static $current_area_level = 0;
  static $area_output = array();
  static $eval_output = array();
  static $areas_to_update = array();
  static $IBE_AJAX_MODE = false;
  static $current_css = array();
  static $trace = "";
  static $paused_content = "";
  static $part_id;
  static $str_template_error = false;
  static $str_template_error_code = false;
  static $arHeadStringsLength;
  static $debug_trace;
  static $area_stack;

  static function IsAjaxMode() {
    return CIBEAjax::$IBE_AJAX_MODE;
  }

  static function AddAjaxURLParams( $url ) {
    if ( CIBEAjax::IsAjaxMode() ) {
      $url .= ( ( strpos( $url, "?" ) === false ) ? "?" : "&" ).CIBEAjax::GetAjaxURLParams();
    }
    return $url;
  }


  static function Init() {
    CIBEAjax::$str_template_error = false;
    if ( isset( $_POST[ "ibe_ajax_mode" ] ) ) {
      CIBEAjax::$IBE_AJAX_MODE = true;
      array_utf8_to_default( $_POST );
      unset( $_POST[ "ibe_ajax_mode" ] );
    }
    if ( isset( $_GET[ "ibe_ajax_mode" ] ) ) {
      CIBEAjax::$IBE_AJAX_MODE = true;
      array_utf8_to_default( $_GET );
      unset( $_GET[ "ibe_ajax_mode" ] );
    }
    if ( CIBEAjax::IsAjaxMode() ) {
      $GLOBALS[ "ibe_trace_sink" ] = "CIBEAjax_Trace";
      AddEventHandler( "main", "OnBeforeProlog", Array( "CIBEAjax", "OnProlog" ) );
      AddEventHandler( "main", "OnAfterEpilog", Array( "CIBEAjax", "OnEpilog" ) );
      if ( isset( $_REQUEST[ "ibe_ajax_update_areas" ] ) ) {
        CIBEAjax::$areas_to_update = explode( ',', $_REQUEST[ "ibe_ajax_update_areas" ] );
      }
    } // !CIBEAjax::IsAjaxMode()
  }

  static function OnProlog() {
    ob_start();
  }

  static function OnEpilog() {
    if ( CIBEAjax::$current_area_level > 0 ) {
    	CIBEAjax::DebugTrace( array( "EndArea missing somewhere", implode( "\r\n", CIBEAjax::$area_stack ) ) );
    }
    //CIBEAjax::DebugTrace( CIBEAjax::$areas_to_update );
    //CIBEAjax::DebugTrace( implode( "\r\n", CIBEAjax::$area_stack ) );
    ob_end_clean();
    /*
    print_r( CIBEAjax::$area_output );
    debug_print_backtrace();
    die();
    */
    //$GLOBALS["APPLICATION"]->RestartBuffer();
    if ( strlen( CIBEAjax::$trace ) > 0 ) {
      end( CIBEAjax::$area_output );
      CIBEAjax::$area_output[ key( CIBEAjax::$area_output ) ][ "DATA" ] .= CIBEAjax::$trace;
      reset( CIBEAjax::$area_output );
      CIBEAjax::$trace = "";
    }
    global $APPLICATION;
    $arParams = array(
      "arResult" => array(
        "AREAS" => &CIBEAjax::$area_output,
        "EVALS" => &CIBEAjax::$eval_output,
        "DEBUG_TRACE" => &CIBEAjax::$debug_trace,
      ),
    );
    if ( CIBEAjax::$str_template_error ) {
      /* см. также ibe_show_error_or_note() */
      switch( CIBEAjax::$str_template_error_code ) {
        case 'NOEXISTITEMS':
        case 'NOEXISTITEMS_DATE':
          /* Ошибка или нет - решается на уровне шаблона */
          CIBEError::Add( CIBEAjax::$str_template_error, CIBEAjax::$str_template_error_code );
          break;
        default:
          CIBEError::Add( CIBEAjax::$str_template_error, CIBEAjax::$str_template_error_code );
          break;
      }
    }
    if ( CIBEErrorExt::GetState( 'ERROR' ) ) {
      CIBEError::Add( CIBEErrorExt::GetString( 'ERROR' ), CIBEErrorExt::GetCode( 'ERROR' ) );
    }
    if ( CIBEErrorExt::GetState( 'NOTE' ) ) {
      $arParams['arResult']['note'] = CIBEErrorExt::GetArray( 'NOTE' );
    }
    if( CIBEDetailedError::Exists() ) {
      $arParams['arResult']['detailed_error'] = CIBEDetailedError::Get();
    }
    if ( CIBEError::IsErrors() ) {
      // Создание $arParams['arResult']['ERROR']
      CIBEError::GetArray( $arParams[ "arResult" ] );
      $arParams['arResult']['ERROR'] = array(
        end( $arParams['arResult']['ERROR'] )
      );
    }
    /* В сесии не должно быть сообщений после завершения работы CIBEAjax */
    CIBEError::Clear();
    CIBEErrorExt::Reset();
    $arParams[ "arResult" ][ "TITLE" ] = $APPLICATION->GetTitle();
    if ( isset( $arParams['arResult']["AREAS"][ "" ] ) ) {
    	// orphan trace (no chance for screen output after trace call)
    	//print_r( $arParams['arResult']["AREAS"][ "" ] );
      $arParams['arResult'][ "ORPHAN_TRACE" ] = $arParams['arResult']["AREAS"][ "" ][ "DATA" ];
      unset( $arParams['arResult']["AREAS"][ "" ] );
    }
    $APPLICATION->IncludeComponent(
      "travelshop:ibe.ajax",
      "",
      $arParams
    );
  }

  static function PauseCurrentArea() {
    global $APPLICATION;
    if (
      CIBEAjax::$current_area_level > 0 &&
      CIBEAjax::IsAjaxMode() &&
      in_array( CIBEAjax::$current_area, CIBEAjax::$areas_to_update )
    ) {
      CIBEAjax::$paused_content = ob_get_contents();
      ob_end_clean();
    }
  }

  static function ResumeCurrentArea() {
    global $APPLICATION;
    if (
      CIBEAjax::$current_area_level > 0 &&
      CIBEAjax::IsAjaxMode() &&
      in_array( CIBEAjax::$current_area, CIBEAjax::$areas_to_update )
    ) {
      ob_start();
      echo CIBEAjax::$paused_content;
      CIBEAjax::$paused_content = "";
    }
  }

  static function StartArea( $area_name ) {
    /*
    if ( $GLOBALS[ "ibe_epilog_started" ] && $area_name == "#ts_ag_offer_filter_container" ) {
      eval( stack() );
      die();
    }
    */
    //trace( $area_name );
    global $APPLICATION;
    $bResult = false;
    if ( CIBEAjax::$current_area_level == 0 ) {
      CIBEAjax::$current_area = $area_name;
      if ( CIBEAjax::IsAjaxMode() && in_array( CIBEAjax::$current_area, CIBEAjax::$areas_to_update ) ) {

        CIBEAjax::$arHeadStringsLength[CIBEAjax::$current_area] = CIBEAjax::CountHeadStrings( $APPLICATION->GetHeadStrings() );
        CIBEAjax::$current_css = explode( '<', $APPLICATION->GetCSS() );

        ob_start();
        echo CIBEAjax::$trace;
        CIBEAjax::$trace = "";
        $bResult = true;
      }
    }
    $bResult = $bResult || ( !CIBEAjax::IsAjaxMode() || CIBEAjax::$current_area_level > 0 || in_array( CIBEAjax::$current_area, CIBEAjax::$areas_to_update ) );
    $stack = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS);
    if ( $bResult ) {
	    CIBEAjax::$area_stack[] = str_repeat( "+", CIBEAjax::$current_area_level + 1 )." ".$stack[0]['file'].":".$stack[0]['line']." (".$area_name.")";
      CIBEAjax::$current_area_level++;
    } else {
    	CIBEAjax::$area_stack[] = str_repeat( "*", CIBEAjax::$current_area_level )." [skipped] ".$stack[0]['file'].":".$stack[0]['line']." (".$area_name.")";
    }
    /* До запуска шаблона зарегистрированы ошибки */
    if ( CIBEError::IsErrors() ) {
      CIBEAjax::$str_template_error = CIBEError::GetString();
      CIBEAjax::$str_template_error_code = CIBEError::GetCode();
    }
    return $bResult;
  }

  static function EndArea() {
    global $APPLICATION;
    $stack = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS);
    CIBEAjax::$area_stack[] = str_repeat( " -", CIBEAjax::$current_area_level )." ".$stack[0]['file'].":".$stack[0]['line'];
    if ( CIBEAjax::$current_area_level == 0 ) {
    	CIBEAjax::DebugTrace( array( "EndArea " . CIBEAjax::$current_area . " called without StartArea", implode( "\r\n", CIBEAjax::$area_stack ) ) );
    }
    CIBEAjax::$current_area_level--;
    if (
      CIBEAjax::$current_area_level == 0 &&
      CIBEAjax::IsAjaxMode() &&
      in_array( CIBEAjax::$current_area, CIBEAjax::$areas_to_update )
    ) {
      /*
      ob_end_clean();
      ob_end_clean();
      ob_end_clean();
      ob_end_clean();
      print_r( CIBEAjax::$current_css );
      print_r( explode( '<', $APPLICATION->GetCSS() ) );
      print_r( array_diff( explode( '<', $APPLICATION->GetCSS() ), CIBEAjax::$current_css ) );
      die();
      */
      $s = "";
      // добавляем новый CSS в вывод компонента
      $new_css = implode( '<', array_diff( explode( '<', $APPLICATION->GetCSS() ), CIBEAjax::$current_css ) );
      if ( strlen( $new_css ) > 0 ) {
        $s .= '<'.$new_css;
      }

      $arHeadStrings = explode( '<', $APPLICATION->GetHeadStrings() );
      $arHeadStrings = array_slice( $arHeadStrings, CIBEAjax::$arHeadStringsLength[CIBEAjax::$current_area] + 1 );
      if ( !empty( $arHeadStrings ) ) {
        $arHeadStrings[0] = '<' . $arHeadStrings[0];
      }

      // добавляем новые HeadStrings в вывод компонента
      $s .= PHP_EOL . trim( implode( '<', $arHeadStrings ) );

      $s .= ob_get_contents();
      ob_end_clean();
      CIBEAjax::$area_output[] = array(
        "MODE" => "replaceContent",
        "SELECTOR" => CIBEAjax::$current_area,
        "DATA" => $s,
        "PART_ID" => CIBEAjax::$part_id,
      );
      /*if ( CIBEAjax::$current_area->GetName() == "travelshop:ibe.offer_filter" ) {
        print_r( CIBEAjax::$area_output );
        die();
      }*/

      /* В шаблоне зарегистрированы ошибки */
      if ( CIBEError::IsErrors() ) {
        CIBEAjax::$str_template_error = CIBEError::GetString();
        CIBEAjax::$str_template_error_code = CIBEError::GetCode();
      }
    }
  }

  static function GetAjaxURLParams() {
    return "ibe_ajax_mode=1&ibe_ajax_update_areas=".urlencode( implode( ",", CIBEAjax::$areas_to_update ) );
  }

  static function SetPartID( $id ) {
    CIBEAjax::$part_id = $id;
  }

  static function RemovePart( $id ) {
    $new_area_output = array();
    foreach ( CIBEAjax::$area_output as &$part ) {
      if ( $part[ "PART_ID" ] != $id ) {
        $new_area_output[] =& $part;
        unset( $part );
      }
    }
    unset( $part );
    CIBEAjax::$area_output =& $new_area_output;
    unset( $new_area_output );
  }

  static function CountHeadStrings( $s ) {
    return substr_count( $s, '<' );
  }

  static function AddEval( $s ) {
    CIBEAjax::$eval_output[] = $s;
  }
  
	static function __GetCurrentAreaLevel() {
		return CIBEAjax::$current_area_level;
	}
	
	static function DebugTrace( $v ) {
		$old_sink = $GLOBALS[ "ibe_trace_sink" ];
		$GLOBALS[ "ibe_trace_sink" ] = false;
		ob_start();
	  static $trace_number = 0;
	  $trace_number++;
	  echo "ajax debug trace #".$trace_number."<br/>";
	  trace( $v );
    CIBEAjax::$debug_trace .= ob_get_contents();
		ob_end_flush();
		$GLOBALS[ "ibe_trace_sink" ] = $old_sink;
	}

} // CIBEAjax

function CIBEAjax_Trace( $v ) {
  $GLOBALS[ "ibe_trace_sink" ] = false;
  if ( CIBEAjax::__GetCurrentAreaLevel() == 0 ) {
    ob_start();
  }
  static $trace_number = 0;
  $trace_number++;
  echo "ajax trace #".$trace_number."<br/>";
  trace( $v );
  if ( CIBEAjax::__GetCurrentAreaLevel() == 0 ) {
    CIBEAjax::$trace .= ob_get_contents();
    ob_end_flush();
  }
  $GLOBALS[ "ibe_trace_sink" ] = "CIBEAjax_Trace";
}

CIBEAjax::Init();
