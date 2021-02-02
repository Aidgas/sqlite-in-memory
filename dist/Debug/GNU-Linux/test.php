<?php


$ch = curl_init();

curl_setopt($ch, CURLOPT_URL,"http://localhost:8787/");
curl_setopt($ch, CURLOPT_POST, 1);
curl_setopt($ch, CURLOPT_POSTFIELDS,
            "cmd=query&value=SELECT name FROM sqlite_master WHERE type='table';");

// in real life you should use something like:
// curl_setopt($ch, CURLOPT_POSTFIELDS, 
//          http_build_query(array('postvar1' => 'value1')));

// receive server response ...
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

$server_output = curl_exec ($ch);

curl_close ($ch);

print_r( json_decode( $server_output ) );

//----------------------------------------------------------------------------------

$ch = curl_init();

curl_setopt($ch, CURLOPT_URL,"http://localhost:8787/");
curl_setopt($ch, CURLOPT_POST, 1);
curl_setopt($ch, CURLOPT_POSTFIELDS,
            "cmd=query&value=SELECT * FROM chat_comments_search WHERE search_text MATCH 'мы'");

// in real life you should use something like:
// curl_setopt($ch, CURLOPT_POSTFIELDS, 
//          http_build_query(array('postvar1' => 'value1')));

// receive server response ...
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

$server_output = curl_exec ($ch);

curl_close ($ch);

print_r( json_decode( $server_output ) );
