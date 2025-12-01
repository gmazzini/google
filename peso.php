<?php

if($_GET["access"]<>"xxxxx")exit(0);
date_default_timezone_set("UTC");
$dt=(int)(time()/86400)-20080;
$val=$_GET["peso"];
$access_token=file_get_contents("/home/www/data/access_token");
$curlPost='
{
  "valueInputOption": "RAW",
  "data": [{
      "range": "data!A'.$dt.'",
      "majorDimension": "ROWS",
      "values": [['.$val.']]
    }]
}';
$ch=curl_init();
curl_setopt($ch,CURLOPT_URL,"https://sheets.googleapis.com/v4/spreadsheets/1sYVvRuPgwsWKTqvNYzG0s5G7OsKSmdsdaGImIuesy0Q/values:batchUpdate");
curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
curl_setopt($ch,CURLOPT_POST,1);
curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Content-Type: application/json","Authorization: Bearer ".$access_token));
curl_setopt($ch,CURLOPT_POSTFIELDS,$curlPost);
curl_exec($ch);
curl_close($ch);

?>
