<?php
$ff=$argv[1];
if($ff==0)$ff=(int)(time()/86400);
$access_token=file_get_contents("/mybind/counted/access_token");
$target_file="/mybind/counted/$ff";
$file_content=file_get_contents($target_file);
$mime_type=mime_content_type($target_file);

$ch=curl_init();
curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
curl_setopt($ch,CURLOPT_POST,1);
curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Content-Type: ".$mime_type,"Authorization: Bearer ".$access_token));
curl_setopt($ch,CURLOPT_POSTFIELDS,$file_content);
$oo=json_decode(curl_exec($ch),true);
print_r($oo);
curl_close($ch);

$file_id=$oo["id"];
$ch=curl_init();
curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files/$file_id?addParents=1wpSVpIUKsd_H2Mnzh51kgQf3EkKOKFLF");
curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
curl_setopt($ch,CURLOPT_POST,1);
curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
curl_setopt($ch,CURLOPT_HTTPHEADER,array("Content-Type: application/json","Authorization: Bearer ".$access_token));
curl_setopt($ch,CURLOPT_CUSTOMREQUEST,"PATCH");
curl_setopt($ch,CURLOPT_POSTFIELDS,'{"name": "'.$ff.'.csv"}');
echo curl_exec($ch);
echo "\n";
curl_close($ch);

?>
