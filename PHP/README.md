- [database_deadlock_recovery.php](database_deadlock_recovery.php)

  DBMS deadlock recovery mechanics.
  
  After the deadlock victim transaction fails, it is re-done again using buffered SQL statements history.

- [ibe_ajax.php](ibe_ajax.php)
  
  AJAX helper for the traditional web application.
  
  Only relevant fragments of the full HTML page rendered by the server business logic are transferred to the browser via AJAX.

- [ibe_direction_filter.php](ibe_direction_filter.php)
  
  Route applicability description parser.
  
  Route applicability filter defines a set of transfers (routes) between some cities. For every directed transfer between two cities we can determine if the tranfer contained in the set described by the filter.

- [js_challenge.php](js_challenge.php)
  
  [JS challenge](https://www.red-button.net/ddos-glossary/javascript-challenge/) mechanics implementation (DOS protection).
  
