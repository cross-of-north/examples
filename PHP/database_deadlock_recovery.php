<?

class CDatabaseWithDeadlockRecovery extends CDatabase {
  
  // Специально один признак на все экземпляры, чтобы не разбираться, где какое соединение
  // (CDatabase или CDatabasePatch).
  static $bRecoveryMode = false;
  static $old_DB = false;
  static $sleep_time = 5;
  static $repeat_count = 3;
  var $arTransactionQueries = array();
  var $bTransactionActive = false;
  var $bQueryLoggingEnabled = true;
  
  // Эта функция вызывается из start.php -> $DB->Connect() -> after_connect.php
  // Если все нормально, то на выходе из after_connect.php в глобальной $DB уже не то, что было на входе.
  // Настройки копируются из созданного ранее экземпляра CDatabase.
  static function Instantiate() {
    
    global $DB;
    
    if ( !( $DB instanceof CDatabaseWithDeadlockRecovery ) ) {
    
      // copy of DB init code from modules/main/start.php
      $new_DB = new CDatabaseWithDeadlockRecovery;
      $new_DB->debug = $DB->debug;
      $new_DB->DebugToFile = $DB->DebugToFile;
      $new_DB->ShowSqlStat = $DB->ShowSqlStat;
      
      CDatabaseWithDeadlockRecovery::$old_DB = $DB;
      $DB = $new_DB;
      $GLOBALS["DB"] = $new_DB;
      
      if ( $new_DB->Connect( 
        CDatabaseWithDeadlockRecovery::$old_DB->DBHost, 
        CDatabaseWithDeadlockRecovery::$old_DB->DBName, 
        CDatabaseWithDeadlockRecovery::$old_DB->DBLogin, 
        CDatabaseWithDeadlockRecovery::$old_DB->DBPassword
      ) ) {
        // иначе соединений будет в два раза больше
        mysql_close( $new_DB->db_Conn );
        $new_DB->db_Conn = CDatabaseWithDeadlockRecovery::$old_DB->db_Conn;
      } else {
        $DB = CDatabaseWithDeadlockRecovery::$old_DB;
        $GLOBALS["DB"] = CDatabaseWithDeadlockRecovery::$old_DB;
      }
    }
    
  }
  
  function Query( $strSql, $bIgnoreErrors = false, $error_position = "", $p1 = false, 
    $p2 = false, $p3 = false, $p4 = false, $p5 = false, $p6 = false, $p7 = false, $p8 = false, $p9 = false ) {
      
    static $deadlock_filename = "dbdeadlock.log";
      
    $original_query = $strSql;
    $result = parent::Query( $strSql.
      ( 
        ( defined( "DB_SOURCE_STAMPS" ) && DB_SOURCE_STAMPS ) ? 
          " -- ".$_SERVER['REMOTE_ADDR'].(
            ( isset( $GLOBALS[ "USER" ] ) && $GLOBALS[ "USER" ]->IsAuthorized() ) ? " ".$this->ForSQL( $GLOBALS[ "USER" ]->GetLogin() ) : ""  
          ).( 
            isset( $GLOBALS[ "APPLICATION" ] ) ? " ".$this->ForSQL( $GLOBALS[ "APPLICATION" ]->GetCurUri() ) : ""
          ) 
        :
          ""
      ), $bIgnoreErrors || CDatabaseWithDeadlockRecovery::$bRecoveryMode, $error_position,
      $p1, $p2, $p3, $p4, $p5, $p6, $p7, $p8, $p9 );

    if ( $result === false ) {
      // ошибка выполнения запроса
      if ( CDatabaseWithDeadlockRecovery::$bRecoveryMode && !$bIgnoreErrors ) {
        // попробуем что-то сделать
        
        // чтобы не протоколировались служебные запросы (например, START TRANSACTION)
        $this->bQueryLoggingEnabled = false;
        
        // http://php.net/manual/en/function.mysql-error.php
        // Be aware that if you are using multiple MySQL connections you MUST support the link identifier to the mysql_error() function. 
        // Otherwise your error message will be blank.
        $this->db_Error = mysql_error( $this->db_Conn );

        if ( 
          strpos( $this->db_Error, "Lock wait timeout exceeded" ) !== false ||
          strpos( $this->db_Error, "Deadlock found when trying to get lock" ) !== false
        ) {
          // откат и повторение транзакции может помочь
          $original_error = get_db_error( $original_query, $this->db_Error );
          log_db_string( "RECOVERABLE ".$original_error, $deadlock_filename );
          
          for ( $i = 1; $i <= CDatabaseWithDeadlockRecovery::$repeat_count; $i++ ) {
          
            log_db_string( "***** RECOVERY (PASS ".$i.") *****\r\n", $deadlock_filename );

            if ( $this->bTransactionActive ) {
              log_db_string( "      Rollback\r\n", $deadlock_filename );
            }
            parent::Rollback(); // на всякий случай без проверки $this->bTransactionActive
            Sleep( CDatabaseWithDeadlockRecovery::$sleep_time );
            $result = true;
            if ( $this->bTransactionActive ) {
              log_db_string( "      StartTransaction\r\n", $deadlock_filename );
              parent::StartTransaction();
              foreach ( $this->arTransactionQueries as $strSql ) {
                log_db_string( "      Repeat: ".$strSql."\r\n", $deadlock_filename );
                $result = parent::Query( $strSql, true );
                if ( $result === false ) {
                  break;
                }
              }
            }
            if ( $result !== false ) {
              log_db_string( "      Repeat: ".$original_query."\r\n", $deadlock_filename );
              $strSql = $original_query;
              $result = parent::Query( $strSql, true, $error_position, $p1, $p2, $p3, $p4, $p5, $p6, $p7, $p8, $p9 );
            }
            if ( $result === false ) {
              log_db_string( "      Repeat failed: ".mysql_error( $this->db_Conn )."\r\n", $deadlock_filename );
            }
            if ( $result === false ) {
              log_db_string( "***** RECOVERY (PASS ".$i.") FAILED *****\r\n", $deadlock_filename );
            } else {
              log_db_string( "***** RECOVERY DONE *****\r\n", $deadlock_filename );
              break;
            }
            
          } // for

          if ( $result === false ) {
            log_db_string( "***** RECOVERY FAILED *****\r\n", $deadlock_filename );
            log_db_string( "RECOVERY FAILED FOR ".$original_error );
          }
          
        }
        if ( $result === false && !$bIgnoreErrors ) {
          // ничего не поможет, и игнорировать тоже нельзя
          if(file_exists($_SERVER["DOCUMENT_ROOT"].BX_PERSONAL_ROOT."/php_interface/dbquery_error.php"))
            include($_SERVER["DOCUMENT_ROOT"].BX_PERSONAL_ROOT."/php_interface/dbquery_error.php");
          elseif(file_exists($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/main/include/dbquery_error.php"))
            include($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/main/include/dbquery_error.php");
          else
            die("MySQL Query Error!");
          die();
        }
        
        $this->bQueryLoggingEnabled = true;
        
      } else {
        // сверху передали $bIgnoreErrors = true
      }
    } 
    
    if ( $result !== false && $this->bTransactionActive && $original_query !== "SELECT @@tx_isolation" && $this->bQueryLoggingEnabled ) {
      // запрос выполнен успешно
      $this->arTransactionQueries[] = $original_query;
    }
    
    return $result;
  }
  
  function StartTransaction( $p1 = false, $p2 = false, $p3 = false, $p4 = false, $p5 = false, $p6 = false, $p7 = false, $p8 = false, $p9 = false ) {
    $result = parent::StartTransaction( $p1, $p2, $p3, $p4, $p5, $p6, $p7, $p8, $p9 );
    $this->arTransactionQueries = array();
    $this->bTransactionActive = true;
    return $result;
  }
  
  function Commit( $p1 = false, $p2 = false, $p3 = false, $p4 = false, $p5 = false, $p6 = false, $p7 = false, $p8 = false, $p9 = false ) {
    $result = parent::Commit( $p1, $p2, $p3, $p4, $p5, $p6, $p7, $p8, $p9 );
    $this->arTransactionQueries = array();
    $this->bTransactionActive = false;
    return $result;
  }
  
  function Rollback( $p1 = false, $p2 = false, $p3 = false, $p4 = false, $p5 = false, $p6 = false, $p7 = false, $p8 = false, $p9 = false ) {
    $result = parent::Rollback( $p1, $p2, $p3, $p4, $p5, $p6, $p7, $p8, $p9 );
    $this->arTransactionQueries = array();
    $this->bTransactionActive = false;
    return $result;
  }
  
  static function SetRecoveryMode( $bRecoveryMode ) {
    CDatabaseWithDeadlockRecovery::$bRecoveryMode = $bRecoveryMode;
  }
  
  static function Test() {
    require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/ibe/ibe_polling_counter.php");
    require_once($_SERVER["DOCUMENT_ROOT"]."/bitrix/modules/ibe/classes/ibe/ibe_airticket.php");

    global $DB;

    $DB->StartTransaction();

    $filename = $_SERVER['DOCUMENT_ROOT']."/deadlock_test_".getmypid().".flag";
    $f = fopen( $filename, "w+" );
    fclose( $f );

    $i = 0;
    while ( file_exists( $filename ) ) {
      CDatabaseWithDeadlockRecovery::SetRecoveryMode( true );
      $DB->Query( "SELECT * FROM ibe_airticket WHERE ORDID=".rand( 115, 139 )." FOR UPDATE" );
      CDatabaseWithDeadlockRecovery::SetRecoveryMode( true );
      CIBEAirTicket::Add( rand(), array( 
      array( "ORDID" => rand(), "DOCNUMBER" => rand(), "PSGID" => rand(), "SEGMENTS" => array( rand() ) ),
      array( "ORDID" => rand(), "DOCNUMBER" => rand(), "PSGID" => rand(), "SEGMENTS" => array( rand() ) ),
    ) );
      CDatabaseWithDeadlockRecovery::SetRecoveryMode( true );
      CIBEPollingCounter::Inc( $i );
      log_db_string( "tick ".getmypid()." (".$i.")\r\n", "dbdeadlock.log" );
      $i++;
      Sleep( rand( 0, 2 ) );
      //break;
    }

    //$DB->Commit();

    die();

  }
  
};

?>
