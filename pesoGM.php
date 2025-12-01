<?php

// if($_GET["access"]<>"xxxxx")exit(0);
$dt=(int)(time()/86400)-20080; $val=$_GET["peso"];
exec("/home/tools/spreadsheets_write 1sYVvRuPgwsWKTqvNYzG0s5G7OsKSmdsdaGImIuesy0Q data\!A$dt ROWS $val");

?>
