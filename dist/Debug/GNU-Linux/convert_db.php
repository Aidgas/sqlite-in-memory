<?php

	$db  = new PDO('sqlite:main.db');
	$db2 = new PDO('sqlite:search.db');
	
	$db->exec("PRAGMA synchronous = OFF");
	$db->exec("PRAGMA journal_mode = MEMORY");
	
	$db2->exec("PRAGMA synchronous = OFF");
	$db2->exec("PRAGMA journal_mode = MEMORY");
	
	$db2->exec('CREATE TABLE IF NOT EXISTS `search` (
		  `id`	    INTEGER
		, `full`	TEXT
	);');
	
	// SELECT full, md5, COUNT(*) c FROM search GROUP BY md5 HAVING c > 1;
	
	$res = $db->query('SELECT * FROM places');
	
	$count = 0;
	echo "start\n";
	
	while( $row = $res->fetch(PDO::FETCH_ASSOC) )
	{
		$house_list = trim($row['house_list']);
		$search = trim($row['name_search']);
		$full = trim($row['full']);
		
		if( strlen($house_list) == 0 )
		{
			$db2->exec('INSERT INTO `search`(`id`,`full`) VALUES (\''.$row['id'].'\',\''.$full.'\')');
		}
		else
		{
			$l = explode(',', $house_list);
			
			foreach($l as $v)
			{
				$db2->exec('INSERT INTO `search`(`id`,`full`) VALUES (\''.$row['id'].'\',\''.$full.', '.$v.'\')');
			}
		}
	
		$count += 1;
		
		echo $count."\r";
		//break;
	}
	
	
	/*$db2->exec('CREATE TABLE IF NOT EXISTS `search` (
		`id`	INTEGER,
		`s1`    TEXT,
		`s2`    TEXT,
		`s3`    TEXT,
		`s4`    TEXT,
		`s5`    TEXT,
		`s6`    TEXT,
		`s7`    TEXT,
		`s8`    TEXT,
		`s9`    TEXT,
		`s10`    TEXT,
		`s11`    TEXT,
		`s12`    TEXT,
		`s13`    TEXT,
		`s14`    TEXT,
		`s15`    TEXT,
		`full`	TEXT
	);');
	
	$res = $db->query('SELECT * FROM places');
	
	$count = 0;
	echo "start\n";
	
	$max = 0;
	$last = '';
	$lstr = '';
	
	while( $row = $res->fetch(PDO::FETCH_ASSOC) )
	{
		$house_list = trim($row['house_list']);
		$search = trim($row['name_search']);
		
		$search = str_replace(' - ', ' ', $search); 
		$search = str_replace('г.', ' ', $search); 
		$search = str_replace('п.', ' ', $search); 
		$search = str_replace(',', ' ', $search); 
		$search = str_replace('"', '', $search); 
		$search = preg_replace('!\s+!', ' ', $search);
		
		$search_l = explode(' ', $search);
		
		$s = array();
		$s[0] = '';
		$s[1] = '';
		$s[2] = '';
		$s[3] = '';
		$s[4] = '';
		$s[5] = '';
		$s[6] = '';
		$s[7] = '';
		$s[8] = '';
		$s[9] = '';
		$s[10] = '';
		$s[11] = '';
		$s[12] = '';
		$s[13] = '';
		
		
		/*if( $max < count($search_l) )
		{
			$lstr = $search;
			
			$max  = count($search_l);
			$last = $search_l;
		}
		
		continue * /
		
		$kk = 0;
		foreach($search_l as $v)
		{
			if( strlen(trim($v)) == 0 ) { continue; }
			
			$s[$kk] = trim($v);
			$kk += 1;
		}
		
		$full = trim($row['full']);
		
		if( strlen($house_list) == 0 )
		{
			$db2->exec('INSERT INTO `search`(`id`,`s1`,`s2`,`s3`,`s4`,`s5`,`s6`,`s7`,`s8`,`s9`,`s10`,`s11`,`s12`,`s13`,`s14`,`s15`,`full`)
			 VALUES ('
			 .$row['id']
			 .", '".$s[0]."'"
			 .",'".$s[1]."'"
			 .",'".$s[2]."'"
			 .",'".$s[3]."'"
			 .",'".$s[4]."'"
			 .",'".$s[5]."'"
			 .",'".$s[6]."'"
			 .",'".$s[7]."'"
			 .",'".$s[8]."'"
			 .",'".$s[9]."'"
			 .",'".$s[10]."'"
			 .",'".$s[11]."'"
			 .",'".$s[12]."'"
			 .",'".$s[13]."'"
			 .",''"
			 .",'".$full."'"
			 .")");
		}
		else
		{
			$l = explode(',', $house_list);
			
			foreach($l as $v)
			{
				$q = 'INSERT INTO `search`(`id`,`s1`,`s2`,`s3`,`s4`,`s5`,`s6`,`s7`,`s8`,`s9`,`s10`,`s11`,`s12`,`s13`,`s14`,`s15`,`full`)
					  VALUES ('
						 .$row['id']
						 .",'".$s[0]."'"
						 .",'".$s[1]."'"
						 .",'".$s[2]."'"
						 .",'".$s[3]."'"
						 .",'".$s[4]."'"
						 .",'".$s[5]."'"
						 .",'".$s[6]."'"
						 .",'".$s[7]."'"
						 .",'".$s[8]."'"
						 .",'".$s[9]."'"
						 .",'".$s[10]."'"
						 .",'".$s[11]."'"
						 .",'".$s[12]."'"
						 .",'".$s[13]."'"
						 .",'".$v."'"
						 .",'".$full.", ".$v."'"
					 .")";
				
				//var_dump($q);
				
				$db2->exec($q);
			}
		}
	
		$count += 1;
		
		echo $count."\r";
		
		//if($count > 10)
		//	break;
	}
	
	echo $max;
	print_r($last);
	var_dump($lstr);*/
	
	$db = null;
	$db2 = null;
