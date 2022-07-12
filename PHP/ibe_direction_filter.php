<?  

// Формат строки-фильтра в описан в документе $/products/doc/ТАРИФЫ/fare-basev10_6Н.doc, п.1.2

class CIBEDirectionFilter {
  
  var $m_syntax_tree;
  
  // ********************** парсинг строки-правила ****************
  
  protected function parse_towns( $towns, &$towns_array ) {
    //eval( stack() );
    $towns = str_replace( '-', ',-', $towns );
    $towns = explode( ',', $towns );
    foreach( $towns as $town ) {
      $town = trim( $town );
      if ( strlen( $town ) > 0 ) {
        $bInclude = true;
        if ( $town[ 0 ] == '-' ) {
          $bInclude = false;
          $town = substr( $town, 1 );
        }
        $towns_array[] = array(
          "NAME" => $town,
          "NORMALIZED" => CIBEDirectionFilter::NormalizeCity( $town ),
          "INCLUDE" => $bInclude
        );
      }
    }
  }

  protected function parse_subset( $item, &$syntax_array ) {
    //eval( stack() );
    $bBothWays = true;
    if ( ( $dir_divider_pos = strpos( $item, '>' ) ) !== false ) {
      $item[ $dir_divider_pos ] = '=';
      $bBothWays = false;
    }
    list( $from, $to ) = explode( '=', $item );
    $towns_from = array();
    $towns_to = array();
    $syntax_array[ 'FROM' ] = array();
    $syntax_array[ 'TO' ] = array();
    $this->parse_towns( $from, $syntax_array[ 'FROM' ] );
    $this->parse_towns( $to, $syntax_array[ 'TO' ] );
    $syntax_array[ 'BOTH_WAYS' ] = $bBothWays;
  }

  protected function parse( $s ) {
    //eval( stack() );
    $link_subsets = explode( '|', $s ); // JIRA00014697 - знак ";" заменить на "|"
    //trace( $link_subsets );
    $this->m_syntax_tree = array();
    foreach ( $link_subsets as $link_subset ) {
      $syntax_subset = array();
      $syntax_subset[ 'INCLUDE' ] = array();
      $syntax_subset[ 'EXCLUDE' ] = array();
      list( $subset_include, $subset_exclude ) = explode( '\\', $link_subset );
      if ( strlen( trim( $subset_include ) ) > 0 || strlen( trim( $subset_exclude ) ) > 0 ) {
        $this->parse_subset( $subset_include, $syntax_subset[ 'INCLUDE' ] );
        $this->parse_subset( $subset_exclude, $syntax_subset[ 'EXCLUDE' ] );
        $this->m_syntax_tree[] =& $syntax_subset;
      }
      unset( $syntax_subset );
    }
  }
  
  // ********************** обработка ****************
  
  // 1 - разрешен, 0 - без изменений, -1 - запрещен
  protected function ProcessTowns( $name, &$towns ) {
    //eval( stack() );
    $result = 0;
    foreach ( $name as $city_or_airport ) {
      foreach ( $towns as $town ) {
        if ( $town[ 'NAME' ] == '*' || $town[ 'NORMALIZED' ] == $city_or_airport ) {
          $result = ( $town[ 'INCLUDE' ] ? 1 : -1 );
        }
      }
    }
    return $result;
  }

  protected function ProcessSubset( $from, $to, &$subset ) {
    //eval( stack() );
    $bResult = false;
    $from_result = $this->ProcessTowns( $from, $subset[ 'FROM' ] );
    $to_result = $this->ProcessTowns( $to, $subset[ 'TO' ] );
    if ( $from_result == 1 && $to_result == 1 ) {
      $bResult = true;
    } else {
      if ( $subset[ 'BOTH_WAYS' ] ) {
        $from_result = $this->ProcessTowns( $to, $subset[ 'FROM' ] );
        $to_result = $this->ProcessTowns( $from, $subset[ 'TO' ] );
        if ( $from_result == 1 && $to_result == 1 ) {
          $bResult = true;
        }
      }
    }
    return $bResult;
  }

  public function Process( $from, $to ) {
    //eval( stack() );
    $bResult = false;
    if ( !is_array( $from ) ) {
      $from = array( $from );
    }
    if ( !is_array( $to ) ) {
      $to = array( $to );
    }
    foreach( $from as $k => $v ) {
      $from[$k] = CIBEDirectionFilter::NormalizeCity( $from[$k] );
    }
    foreach( $to as $k => $v ) {
      $to[$k] = CIBEDirectionFilter::NormalizeCity( $to[$k] );
    }
    foreach ( $this->m_syntax_tree as $subset ) {
      //trace( $subset );
      $bInclude = $this->ProcessSubset( $from, $to, $subset[ 'INCLUDE' ] );
      if ( $bInclude && !$this->ProcessSubset( $from, $to, $subset[ 'EXCLUDE' ] ) ) {
        $bResult = true;
        break;
      }
    }
    return $bResult;
  }
  
  public function IsEmpty() {
    return count( $this->m_syntax_tree ) == 0;
  }

  public function __construct( $s ) {
    $this->parse( $s );
    //trace( $this );
  }
  
  public static function do_test( $rule, $from, $to, $bShouldPass, $bShouldPassReverse ) {
    $o = new CIBEDirectionFilter( $rule );
    $bPass = $o->Process( $from, $to );
    $bPassReverse = $o->Process( $to, $from );
    echo ("<tr><td style='padding:5px;'>".$rule."</td><td style='padding:5px;'>".$from."-".$to.
      "</td><td style='padding:5px;'>".( $bPass ? "TRUE" : "FALSE" )."</td><td style='padding:5px;'>".
      ( $bPassReverse ? "TRUE" : "FALSE" )."</td><td style='padding:5px;'>".
      ( ( $bPass == $bShouldPass && $bPassReverse == $bShouldPassReverse ) ? "OK" : "FAILED" )."</tr>");
  }

  public static function do_test_text( $rule, $from, $to  ) {
    $o = new CIBEDirectionFilter( $rule );
    $bPass = $o->Process( $from, $to );
    $bPassReverse = $o->Process( $to, $from );
    return sprintf( "Rule: ".$rule.", Segment: ".$from."-".$to.
      ", Direct result: ".( $bPass ? "TRUE" : "FALSE" ).", Reverse result: ".
      ( $bPassReverse ? "TRUE" : "FALSE" ) );
  }

  public static function test() {
    ?><table style="border-width:10px;border-collapse:none;"><?
    echo( "<tr><td style='padding:5px;'>Rule</td><td style='padding:5px;'>Segment</td><td style='padding:5px;'>".
    "Direct result</td><td style='padding:5px;'>Reverse result</td><td>Test OK</td></tr>" );
    CIBEDirectionFilter::do_test( "МОВ=СПТ", "МОВ", "СПТ", true, true );
    CIBEDirectionFilter::do_test( "МОВ=СПТ", "СПТ", "МОВ", true, true );
    CIBEDirectionFilter::do_test( "МОВ>СПТ", "МОВ", "СПТ", true, false );
    CIBEDirectionFilter::do_test( "МОВ>СПТ", "СПТ", "МОВ", false, true );
    CIBEDirectionFilter::do_test( "МОВ,СПТ=СПТ,МОВ", "МОВ", "СПТ", true, true );
    CIBEDirectionFilter::do_test( "МОВ,СПТ=СПТ,МОВ", "СПТ", "МОВ", true, true );
    CIBEDirectionFilter::do_test( "МОВ,СПТ>СПТ,МОВ", "МОВ", "СПТ", true, true );
    CIBEDirectionFilter::do_test( "МОВ,СПТ>СПТ,МОВ", "СПТ", "МОВ", true, true );
    CIBEDirectionFilter::do_test( "МОВ=КГД", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( "МОВ>*", "МОВ", "СПТ", true, false );
    CIBEDirectionFilter::do_test( "*>*", "МОВ", "СПТ", true, true );
    CIBEDirectionFilter::do_test( "*>*-СПТ", "МОВ", "СПТ", false, true );
    CIBEDirectionFilter::do_test( "*=*-СПТ", "МОВ", "СПТ", true, true );
    CIBEDirectionFilter::do_test( "*>*\\МОВ=СПТ", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( "*>*\\МОВ>СПТ", "МОВ", "СПТ", false, true );
    CIBEDirectionFilter::do_test( "*>*\\*=*", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( "*>*\\*>СПТ", "МОВ", "СПТ", false, true );
    CIBEDirectionFilter::do_test( "*-МОВ-СПТ=*", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( "*-МОВ-СПТ>*", "КГД", "СПТ", true, false );
    CIBEDirectionFilter::do_test( "*,-МОВ,-СПТ>*", "КГД", "СПТ", true, false );
    CIBEDirectionFilter::do_test( "*>*\\*=*|*>*\\*=*", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( "*>*\\*=*|МОВ>СПТ", "МОВ", "СПТ", true, false );
    CIBEDirectionFilter::do_test( "МОВ>ТАC|МОВ>КЯА|МОВ>СПТ", "МОВ", "СПТ", true, false );
    CIBEDirectionFilter::do_test( ";;;", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( "\\\\\\", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( ",,,,,", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( ",*,*,*,*,*,*", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( ",,=,,", "МОВ", "СПТ", false, false );
    CIBEDirectionFilter::do_test( "-*-*-*-*-*-", "МОВ", "СПТ", false, false );
    ?></table><?
  }

  /**
   * Узнать город по коду города или по коду аэропорта.
   * Для вывода не используется.
   *
   * @param string $str Строка для нормализации.
   * @return string Строка вида "Город (Gorod)"
   */
  public static function NormalizeCity( $str ) {
    static $arCity = array( );
    $cachekey = $str;

    if ( isset( $arCity[$cachekey] ) ) {
      return $arCity[$cachekey];
    }

    $arSciWhat = SciWhat( array( $str => 'П' ) );

    if ( empty( $arSciWhat ) ) {
      return $str;
    } else {
      foreach ( $arSciWhat as $k => $v ) {
        $str = $arCity[$cachekey] = $v['CITYNAME'] . ' (' . $v['CITYNAMEENGL'] . ')';
      }
    }
    return $str;
  }
  
}