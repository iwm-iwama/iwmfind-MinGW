//------------------------------------------------------------------------------
#define   IWM_VERSION         "iwmfind4_20210603"
#define   IWM_COPYRIGHT       "Copyright (C)2009-2021 iwm-iwama"
//------------------------------------------------------------------------------
#include  "lib_iwmutil.h"
#include  "sqlite3.h"

INT  main();
MBS  *sql_escape(MBS *pM);
VOID ifind10($struct_iFinfoW *FI, WCS *dir, UINT dirLenJ, INT depth);
VOID ifind10_CallCnt(UINT uCnt);
VOID ifind21(WCS *dir, UINT dirId, INT depth);
VOID ifind22($struct_iFinfoW *FI, UINT dirId, WCS *fname);
VOID sql_exec(sqlite3 *db, const MBS *sql, sqlite3_callback cb);
INT  sql_columnName(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
INT  sql_result_std(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
INT  sql_result_countOnly(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
INT  sql_result_exec(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
BOOL sql_saveOrLoadMemdb(sqlite3 *mem_db, MBS *ofn, BOOL save_load);
VOID print_footer();
VOID print_version();
VOID print_help();

//-----------
// 共用変数
//-----------
INT  $i1 = 0, $i2 = 0;
MBS  *$p1 = 0, *$p2 = 0;
MBS  **$ap1 = {0}, **$ap2 = {0}, **$ap3 = {0};
MBS  *$sBuf = 0;            // Tmp文字列
UINT $uBuf = 0;             // $sBufの文字長
#define   BUF_SIZE_MAX        (1024 * 100)
#define   BUF_SIZE_DMZ        (2048)
UINT $uDirId = 0;           // Dir数
UINT $uAllCnt = 0;          // 検索数
UINT $uCallCnt_ifind10 = 0; // ifind10()が呼ばれた回数
UINT $uStepCnt = 0;         // CurrentDir位置
UINT $uRowCnt = 0;          // 処理行数
U8N  *$sqlU = 0;            // SQL
sqlite3      *$iDbs = 0;
sqlite3_stmt *$stmt1 = 0, *$stmt2 = 0;

// [文字色] + ([背景色] * 16)
//  0 = Black    1 = Navy     2 = Green    3 = Teal
//  4 = Maroon   5 = Purple   6 = Olive    7 = Silver
//  8 = Gray     9 = Blue    10 = Lime    11 = Aqua
// 12 = Red     13 = Fuchsia 14 = Yellow  15 = White

// タイトル
#define   COLOR01             (15 + ( 9 * 16))
// 入力例／注
#define   COLOR11             (15 + (12 * 16))
#define   COLOR12             (13 + ( 0 * 16))
#define   COLOR13             (12 + ( 0 * 16))
// 引数
#define   COLOR21             (14 + ( 0 * 16))
#define   COLOR22             (11 + ( 0 * 16))
// 説明
#define   COLOR91             (15 + ( 0 * 16))
#define   COLOR92             ( 7 + ( 0 * 16))

#define   MEMDB     ":memory:"
#define   OLDDB     ("iwmfind.db."IWM_VERSION)
#define   CREATE_T_DIR \
			"CREATE TABLE T_DIR( \
				dir_id INTEGER, \
				dir TEXT, \
				depth INTEGER, \
				step_byte INTEGER \
			);"
#define   INSERT_T_DIR \
			"INSERT INTO T_DIR( \
				dir_id, \
				dir, \
				depth, \
				step_byte \
			) VALUES(?, ?, ?, ?);"
#define   CREATE_T_FILE \
			"CREATE TABLE T_FILE( \
				id        INTEGER, \
				dir_id    INTEGER, \
				name      TEXT, \
				ext_pos   INTEGER, \
				attr_num  INTEGER, \
				ctime_cjd DOUBLE, \
				mtime_cjd DOUBLE, \
				atime_cjd DOUBLE, \
				size      INTEGER, \
				number    INTEGER, \
				flg       INTEGER \
			);"
#define   INSERT_T_FILE \
			"INSERT INTO T_FILE( \
				id, \
				dir_id, \
				name, \
				ext_pos, \
				attr_num, \
				ctime_cjd, \
				mtime_cjd, \
				atime_cjd, \
				size, \
				number, \
				flg \
			) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
#define   CREATE_VIEW \
			"CREATE VIEW V_INDEX AS SELECT \
				T_FILE.id AS id, \
				(T_DIR.dir || T_FILE.name) AS path, \
				T_DIR.dir AS dir, \
				T_FILE.name AS name, \
				substr(T_FILE.name, T_FILE.ext_pos + 1) AS ext, \
				(CASE T_FILE.ext_pos WHEN 0 THEN 'd' ELSE 'f' END) AS type, \
				T_FILE.attr_num AS attr_num, \
				((CASE T_FILE.ext_pos WHEN 0 THEN 'd' ELSE 'f' END) || (CASE 1&T_FILE.attr_num WHEN 1 THEN 'r' ELSE '-' END) || (CASE 2&T_FILE.attr_num WHEN 2 THEN 'h' ELSE '-' END) || (CASE 4&T_FILE.attr_num WHEN 4 THEN 's' ELSE '-' END) || (CASE 32&T_FILE.attr_num WHEN 32 THEN 'a' ELSE '-' END)) AS attr, \
				T_FILE.ctime_cjd AS ctime_cjd, \
				datetime(T_FILE.ctime_cjd - 0.5) AS ctime, \
				T_FILE.mtime_cjd AS mtime_cjd, \
				datetime(T_FILE.mtime_cjd - 0.5) AS mtime, \
				T_FILE.atime_cjd AS atime_cjd, \
				datetime(T_FILE.atime_cjd - 0.5) AS atime, \
				T_FILE.size AS size, \
				T_FILE.number AS LN, \
				T_FILE.flg AS flg, \
				depth, \
				step_byte \
				FROM T_FILE LEFT JOIN T_DIR ON T_FILE.dir_id=T_DIR.dir_id;"
#define   UPDATE_EXEC99_1 \
			"UPDATE T_FILE SET flg=1 WHERE id IN (SELECT id FROM V_INDEX %s);"
#define   DELETE_EXEC99_1 \
			"DELETE FROM T_FILE WHERE flg IS NULL;"
#define   DELETE_EXEC99_2 \
			"DELETE FROM T_FILE WHERE (dir_id IN (SELECT dir_id FROM T_DIR WHERE dir LIKE '%\\System Volume Information\\%')) AND ext_pos!=0;"
#define   UPDATE_EXEC99_2 \
			"UPDATE T_FILE SET flg=NULL;"
#define   SELECT_VIEW \
			"SELECT %s FROM V_INDEX %s %s %s ORDER BY %s;"
#define   OP_SELECT_0 \
			"LN,path"
#define   OP_SELECT_1 \
			"LN,path,depth,type,size,ctime,mtime,atime"
#define   OP_SELECT_MKDIR \
			"step_byte,dir,name,path"
#define   OP_SELECT_EXTRACT \
			"path,name"
#define   OP_SELECT_RP \
			"type,path"
#define   OP_SELECT_RM \
			"path,dir,attr_num"
#define   I_MKDIR    1
#define   I_CP       2
#define   I_MV       3
#define   I_MV2      4
#define   I_EXT1     5
#define   I_EXT2     6
#define   I_RP      11
#define   I_RM      21
#define   I_RM2     22
//
// 現在時間
//
INT *$aiNow = 0;
//
// 検索dir
//
MBS **$aDirList = {0};
UINT $aDirListSize = 0;
//
// 入力DB
// -in=STR | -i=STR
//
MBS *$sIn = "";
MBS *$sInDbn = MEMDB;
//
// 出力DB
// -out=STR | -o=STR
//
MBS *$sOut = "";
MBS *$sOutDbn = "";
//
// select 句
// -select=STR | -s=STR
//
MBS *$sSelect = OP_SELECT_0;
INT $iSelectPosNumber = 0; // "LN"の配列位置
//
// where 句
// -where=STR | -w=STR
//
MBS *$sWhere0 = "";
MBS *$sWhere1 = "WHERE";
MBS *$sWhere2 = "";
//
// group by 句
// -group=STR | -g=STR
//
MBS *$sGroup = "";
//
// order by 句
// -sort=STR | -st=STR
//
MBS *$sSort = "";
//
// ヘッダ情報を非表示
// -noheader | -nh
//
BOOL $bNoHeader = FALSE;
//
// フッタ情報を非表示
// -nofooter | -nf
//
BOOL $bNoFooter = FALSE;
//
// 出力を STR で囲む
// -quote=STR | -qt=STR
//
MBS *$sQuote = "";
//
// 出力を STR で区切る
// -separate=STR | -sp=STR
//
MBS *$sSeparate = " | ";
//
// 検索Dir位置
// -depth=NUM1,NUM2 | -d=NUM1,NUM2
//
INT $iDepthMin = 0; // 下階層〜開始位置(相対)
INT $iDepthMax = 0; // 下階層〜終了位置(相対)
//
// mkdir STR
// --mkdir=STR | --md=STR
//
MBS *$sMd = "";
MBS *$sMdOp = "";
//
// mkdir STR & copy
// --copy=STR | --cp=STR
//
MBS *$sCp = "";
//
// mkdir STR & move STR
// --move=STR | --mv=STR
//
MBS *$sMv = "";
//
// mkdir STR & move STR & rmdir
// --move2=STR | --mv2=STR
//
MBS *$sMv2 = "";
//
// copy STR
// --extract1=STR | --ext1=STR
//
MBS *$sExt1 = "";
//
// move STR
// --extract2=STR | --ext2=STR
//
MBS *$sExt2 = "";
//
// replace results with STR
// --replace=STR | --rep=STR
//
MBS *$sRep = "";
MBS *$sRepOp = "";
//
// --rm  | --remove
// --rm2 | --remove2
//
INT $iRm = 0;
//
// 優先順 (安全)I_MD > (危険)I_RM2
//  I_MD  = --mkdir
//  I_CP  = --cp
//  I_MV  = --mv
//  I_MV2 = --mv2
//  I_RP  = --rep
//  I_RM  = --rm
//  I_RM2 = --rm2
//
INT $iExec = 0;

INT
main()
{
	// lib_iwmutil 初期化
	iCLI_getCMD();       //=> $IWM_CMD
	iCLI_getARGS();      //=> $IWM_ARGV, $IWM_ARGC
	iConsole_getColor(); //=> $IWM_ColorDefault, $IWM_StdoutHandle
	iExecSec_init();     //=> $IWM_ExecSecBgn

	// -h | -help
	if(! $IWM_ARGC || imb_cmpp($IWM_ARGV[0], "-h") || imb_cmpp($IWM_ARGV[0], "-help"))
	{
		print_help();
		imain_end();
	}

	// -v | -version
	if(imb_cmpp($IWM_ARGV[0], "-v") || imb_cmpp($IWM_ARGV[0], "-version"))
	{
		print_version();
		imain_end();
	}

	// 大域変数
	$sBuf = icalloc_MBS(BUF_SIZE_MAX + BUF_SIZE_DMZ);

	// 現在時間
	$aiNow = (INT*)idate_cjd_to_iAryYmdhns(idate_nowToCjd(TRUE));

	// [0..]
	/*
		$aDirList取得で $iDepthMax を使うため先に、
			-recursive
			-depth
		をチェック
	*/
	for($i1 = 0; $i1 < $IWM_ARGC; $i1++)
	{
		MBS **_as1 = ija_split($IWM_ARGV[$i1], "=", "\"\"\'\'", FALSE);
		MBS **_as2 = ija_split(_as1[1], ",", "\"\"\'\'", TRUE);
		UINT _as2Size = iary_size(_as2);

		// -r | -recursive
		if(imb_cmpp(_as1[0], "-r") || imb_cmpp(_as1[0], "-recursive"))
		{
			$iDepthMin = 0;
			$iDepthMax = IMAX_PATHA;
		}

		// -d=NUM1,NUM2 | -depth=NUM1,NUM2
		if(imb_cmpp(_as1[0], "-d") || imb_cmpp(_as1[0], "-depth"))
		{
			if(_as2Size > 1)
			{
				$iDepthMin = inum_atoi(_as2[0]);
				$iDepthMax = inum_atoi(_as2[1]);

				if($iDepthMin > $iDepthMax)
				{
					$i2 = $iDepthMin;
					$iDepthMin = $iDepthMax;
					$iDepthMax = $i2;
				}
			}
			else if(_as2Size == 1)
			{
				$iDepthMin = $iDepthMax = inum_atoi(_as2[0]);
			}
			else
			{
				$iDepthMin = $iDepthMax = 0;
			}
		}

		ifree(_as2);
		ifree(_as1);
	}

	INT iArgsPos = 0;

	// [0..n]
	for($i1 = 0; $i1 < $IWM_ARGC; $i1++)
	{
		if(*$IWM_ARGV[$i1] == '-')
		{
			break;
		}
		// dir不在
		if(iFchk_typePathA($IWM_ARGV[$i1]) != 1)
		{
			P("[Err] Dir(%d) '%s' は存在しない!\n", ($i1 + 1), $IWM_ARGV[$i1]);
		}
	}
	iArgsPos = $i1;

	// $aDirList を作成
	if(iArgsPos)
	{
		$ap1 = icalloc_MBS_ary(iArgsPos);
			for($i1 = 0; $i1 < iArgsPos; $i1++)
			{
				$ap1[$i1] = iFget_AdirA($IWM_ARGV[$i1]); // 絶対PATHへ変換
			}
			// 上位Dirのみ取得
			$aDirList = iary_higherDir($ap1, $iDepthMax);
			$aDirListSize = iary_size($aDirList);
		ifree($ap1);
	}

	// [n..]
	for($i1 = iArgsPos; $i1 < $IWM_ARGC; $i1++)
	{
		MBS **_as1 = ija_split($IWM_ARGV[$i1], "=", "\"\"\'\'", FALSE);
		MBS **_as2 = ija_split(_as1[1], ",", "\"\"\'\'", TRUE);
		UINT _as2Size = iary_size(_as2);

		// -i | -in
		if(imb_cmpp(_as1[0], "-i") || imb_cmpp(_as1[0], "-in"))
		{
			if(_as2Size)
			{
				if(iFchk_typePathA(_as2[0]) != 2)
				{
					P("[Err] -in '%s' は存在しない!\n", _as2[0]);
					imain_end();
				}
				else if($aDirListSize)
				{
					P("[Err] Dir と -in は併用できない!\n");
					imain_end();
				}
				else
				{
					$sIn = ims_clone(_as2[0]);
					$sInDbn = ims_clone($sIn);

					// -in のときは -recursive 自動付与
					$iDepthMin = 0;
					$iDepthMax = IMAX_PATHA;
				}
			}
		}

		// -o | -out
		if(imb_cmpp(_as1[0], "-o") || imb_cmpp(_as1[0], "-out"))
		{
			if(_as2Size)
			{
				$sOut = ims_clone(_as2[0]);
				$sOutDbn = ims_clone($sOut);
			}
		}

		// --md | --mkdir
		if(imb_cmpp(_as1[0], "--md") || imb_cmpp(_as1[0], "--mkdir"))
		{
			if(_as2Size)
			{
				$sMd = ims_clone(_as2[0]);
			}
		}

		// --cp | --copy
		if(imb_cmpp(_as1[0], "--cp") || imb_cmpp(_as1[0], "--copy"))
		{
			if(_as2Size)
			{
				$sCp = ims_clone(_as2[0]);
			}
		}

		// --mv | --move
		if(imb_cmpp(_as1[0], "--mv") || imb_cmpp(_as1[0], "--move"))
		{
			if(_as2Size)
			{
				$sMv = ims_clone(_as2[0]);
			}
		}

		// --mv2 | --move2
		if(imb_cmpp(_as1[0], "--mv2") || imb_cmpp(_as1[0], "--move2"))
		{
			if(_as2Size)
			{
				$sMv2 = ims_clone(_as2[0]);
			}
		}

		// --ext1 | --extract1
		if(imb_cmpp(_as1[0], "--ext1") || imb_cmpp(_as1[0], "--extract1"))
		{
			if(_as2Size)
			{
				$sExt1 = ims_clone(_as2[0]);
			}
		}

		// --ext2 | --extract2
		if(imb_cmpp(_as1[0], "--ext2") || imb_cmpp(_as1[0], "--extract2"))
		{
			if(_as2Size)
			{
				$sExt2 = ims_clone(_as2[0]);
			}
		}

		// --rep | --replace
		if(imb_cmpp(_as1[0], "--rep") || imb_cmpp(_as1[0], "--replace"))
		{
			if(_as2Size)
			{
				$sRep = ims_clone(_as2[0]);
			}
		}

		// --rm | --remove
		if(imb_cmpp(_as1[0], "--rm") || imb_cmpp(_as1[0], "--remove"))
		{
			$iRm = I_RM;
		}

		// --rm2 | --remove2
		if(imb_cmpp(_as1[0], "--rm2") || imb_cmpp(_as1[0], "--remove2"))
		{
			$iRm = I_RM2;
		}

		// -s | -select
		/*
			(none)         : OP_SELECT_0
			-select (none) : OP_SELECT_1
			-select=COLUMN1,COLUMN2,...
		*/
		if(imb_cmpp(_as1[0], "-s") || imb_cmpp(_as1[0], "-select"))
		{
			if(_as2Size)
			{
				// "LN"位置を求める
				$ap2 = ija_token(_as2[0], ", ");
					$ap3 = iary_simplify($ap2, TRUE); // LN表示は１個しか出来ないので重複排除
						$iSelectPosNumber = 0;
						while(($p2 = $ap3[$iSelectPosNumber]))
						{
							if(imb_cmppi($p2, "LN"))
							{
								break;
							}
							++$iSelectPosNumber;
						}
						if(! $p2)
						{
							$iSelectPosNumber = -1;
						}
						$sSelect = iary_join($ap3, ",");
					ifree($ap3);
				ifree($ap2);
			}
			else
			{
				$sSelect = OP_SELECT_1;
			}
		}

		// -st | -sort
		if(imb_cmpp(_as1[0], "-st") || imb_cmpp(_as1[0], "-sort"))
		{
			if(_as2Size)
			{
				$sSort = ims_clone(_as2[0]);
			}
		}

		// -w | -where
		if(imb_cmpp(_as1[0], "-w") || imb_cmpp(_as1[0], "-where"))
		{
			if(_as2Size)
			{
				$sWhere0 = sql_escape(_as2[0]);
				sprintf($sBuf, "WHERE %s AND", $sWhere0);
				$sWhere1 = ims_clone($sBuf);
			}
		}

		// -g | -group
		if(imb_cmpp(_as1[0], "-g") || imb_cmpp(_as1[0], "-group"))
		{
			if(_as2Size)
			{
				sprintf($sBuf, "GROUP BY %s", _as2[0]);
				$sGroup = ims_clone($sBuf);
			}
		}

		// -nh | -noheader
		if(imb_cmpp(_as1[0], "-nh") || imb_cmpp(_as1[0], "-noheader"))
		{
			$bNoHeader = TRUE;
		}

		// -nf | -nofooter
		if(imb_cmpp(_as1[0], "-nf") || imb_cmpp(_as1[0], "-nofooter"))
		{
			$bNoFooter = TRUE;
		}

		// -qt | -quote
		if(imb_cmpp(_as1[0], "-qt") || imb_cmpp(_as1[0], "-quote"))
		{
			if(_as2Size)
			{
				$sQuote = ims_conv_escape(_as2[0]);
			}
		}

		// -sp | -separate
		if(imb_cmpp(_as1[0], "-sp") || imb_cmpp(_as1[0], "-separate"))
		{
			if(_as2Size)
			{
				$sSeparate = ims_conv_escape(_as2[0]);
			}
		}

		ifree(_as2);
		ifree(_as1);
	}

	// Err
	if(! $aDirListSize && ! *$sIn)
	{
		P("[Err] Dir もしくは -in を指定してください!\n");
		imain_end();
	}

	// --[exec] 関係を一括変換
	if(*$sMd || *$sCp || *$sMv || *$sMv2 || *$sExt1 || *$sExt2 || *$sRep || $iRm)
	{
		$bNoFooter = TRUE; // フッタ情報を非表示

		if(*$sMd)
		{
			$iExec = I_MKDIR;
			$sMdOp = ims_clone($sMd);
		}
		else if(*$sCp)
		{
			$iExec = I_CP;
			$sMdOp = ims_clone($sCp);
		}
		else if(*$sMv)
		{
			$iExec = I_MV;
			$sMdOp = ims_clone($sMv);
		}
		else if(*$sMv2)
		{
			$iExec = I_MV2;
			$sMdOp = ims_clone($sMv2);
		}
		else if(*$sExt1)
		{
			$iExec = I_EXT1;
			$sMdOp = ims_clone($sExt1);
		}
		else if(*$sExt2)
		{
			$iExec = I_EXT2;
			$sMdOp = ims_clone($sExt2);
		}
		else if(*$sRep)
		{
			if(iFchk_typePathA($sRep) != 2)
			{
				P("[Err] --replace '%s' は存在しない!\n", $sRep);
				imain_end();
			}
			else
			{
				$iExec = I_RP;
				$sRepOp = ims_clone($sRep);
			}
		}
		else if($iRm)
		{
			$iExec = $iRm;
		}

		if($iExec <= I_EXT2)
		{
			$p1 = iFget_AdirA($sMdOp);
				$sMdOp = ijs_cutR($p1, "\\");
			ifree($p1);
			$sSelect = ($iExec <= I_MV2 ? OP_SELECT_MKDIR : OP_SELECT_EXTRACT);
		}
		else if($iExec == I_RP)
		{
			$sSelect = OP_SELECT_RP;
		}
		else
		{
			$sSelect = OP_SELECT_RM;
		}
	}

	// -where 関係を一括変換
	sprintf($sBuf, "depth>=%d AND depth<=%d", $iDepthMin, $iDepthMax);
	$sWhere2 = ims_clone($sBuf);

	// -sort 関係を一括変換
	if($iExec >= I_MV)
	{
		// Dir削除時必要なものは "order by desc"
		ifree($sSort);
		$sSort = "path desc";
	}
	else
	{
		if(! *$sSort)
		{
			ifree($sSort);
			$sSort = "dir, name";
		}
		/*
			path, dir, name, ext
			ソートは、大文字・小文字を区別しない
		*/
		$p1 = ims_clone($sSort);
			$p2 = ijs_replace($p1, "path", "lower(path)");
				ifree($p1);
				$p1 = $p2;
			$p2 = ijs_replace($p1, "dir", "lower(dir)");
				ifree($p1);
				$p1 = $p2;
			$p2 = ijs_replace($p1, "name", "lower(name)");
				ifree($p1);
				$p1 = $p2;
			$p2 = ijs_replace($p1, "ext", "lower(ext)");
				ifree($p1);
				$p1 = $p2;
			$sSort = ims_clone($p1);
		ifree($p1);
	}

	// SQL作成
	// SJIS で作成（DOSプロンプト対応）
	snprintf($sBuf, BUF_SIZE_MAX, SELECT_VIEW, $sSelect, $sWhere1, $sWhere2, $sGroup, $sSort);

	// SJIS を UTF-8 に変換（Sqlite3対応）
	$sqlU = M2U($sBuf);

	// -in DBを指定
	// UTF-8
	U8N *up1 = M2U($sInDbn);
		if(sqlite3_open(up1, &$iDbs))
		{
			P("[Err] -in '%s' を開けない!\n", $sInDbn);
			sqlite3_close($iDbs); // ErrでもDB解放
			imain_end();
		}
		else
		{
			// DB構築
			if(! *$sIn)
			{
				$struct_iFinfoW *FI = iFinfo_allocW();
					/*
						【重要】UTF-8でDB構築
							データ入力(INSERT)は、
								・PRAGMA encoding = 'TTF-16le'
								・bind_text16()
							を使用して、"UTF-16LE"で登録可能だが、
							出力(SELECT)は、exec()が"UTF-8"しか扱わないため本時点では、
								＜入力＞ UTF-16 => UTF-8
								＜SQL＞  SJIS   => UTF-8
								＜出力＞ UTF-8  => SJIS
							の方法に辿り着いた。
					*/
					sql_exec($iDbs, "PRAGMA encoding='UTF-8';", 0);
					// TABLE作成
					sql_exec($iDbs, CREATE_T_DIR, 0);
					sql_exec($iDbs, CREATE_T_FILE, 0);
					// VIEW作成
					sql_exec($iDbs, CREATE_VIEW, 0);
					// 前処理
					sqlite3_prepare($iDbs, INSERT_T_DIR, imi_len(INSERT_T_DIR), &$stmt1, 0);
					sqlite3_prepare($iDbs, INSERT_T_FILE, imi_len(INSERT_T_FILE), &$stmt2, 0);
					// トランザクション開始
					sql_exec($iDbs, "BEGIN", 0);
					// 検索データ DB書込
					for($i2 = 0; ($p1 = $aDirList[$i2]); $i2++)
					{
						WCS *wp1 = M2W($p1);
							$uStepCnt = iwi_len(wp1);
							ifind10(FI, wp1, $uStepCnt, 0); // 本処理
						ifree(wp1);
					}
					// 経過を表示
					ifind10_CallCnt(0);
					// 文字化けを起こすSystemFileを削除
					sql_exec($iDbs, DELETE_EXEC99_2, 0);
					// トランザクション終了
					sql_exec($iDbs, "COMMIT", 0);
					// 後処理
					sqlite3_finalize($stmt2);
					sqlite3_finalize($stmt1);
				iFinfo_freeW(FI);
			}
			// 結果
			if(*$sOut)
			{
				// 存在する場合、削除
				irm_file($sOutDbn);
				// $sIn, $sOut両指定、別ファイル名のとき
				if(*$sIn)
				{
					CopyFile($sInDbn, OLDDB, FALSE);
				}
				// WHERE STR のとき不要データ削除
				if(*$sWhere0)
				{
					sql_exec($iDbs, "BEGIN", 0); // トランザクション開始
						$p1 = ims_cats("WHERE ", $sWhere0, NULL);
							sprintf($sBuf, UPDATE_EXEC99_1, $p1);
						ifree($p1);
						$p1 = M2U($sBuf); // UTF-8で処理
							sql_exec($iDbs, $p1, 0);             // フラグを立てる
							sql_exec($iDbs, DELETE_EXEC99_1, 0); // 不要データ削除
							sql_exec($iDbs, UPDATE_EXEC99_2, 0); // フラグ初期化
						ifree($p1);
					sql_exec($iDbs, "COMMIT", 0); // トランザクション終了
					sql_exec($iDbs, "VACUUM", 0); // VACUUM
				}
				// $sIn, $sOut 両指定のときは, 途中, ファイル名が逆になるので, 後でswap
				sql_saveOrLoadMemdb($iDbs, (*$sIn ? $sInDbn : $sOutDbn), TRUE);
				// outDb
				sql_exec($iDbs, "SELECT LN FROM V_INDEX;", sql_result_countOnly); // "SELECT *" は遅い
			}
			else
			{
				if($iExec)
				{
					sql_exec($iDbs, $sqlU, sql_result_exec);
				}
				else
				{
					// カラム名表示
					if(! $bNoHeader)
					{
						sprintf($sBuf, "SELECT %s FROM V_INDEX WHERE id=1;", $sSelect);
						sql_exec($iDbs, $sBuf, sql_columnName);
					}
					sql_exec($iDbs, $sqlU, sql_result_std);
				}
			}
			// DB解放
			sqlite3_close($iDbs);
			// $sIn, $sOut ファイル名をswap
			if(*$sIn)
			{
				MoveFile($sInDbn, $sOutDbn);
				MoveFile(OLDDB, $sInDbn);
			}
			// 作業用ファイル削除
			DeleteFile(OLDDB);
		}
	ifree(up1);

	// フッタ部
	if(! $bNoFooter)
	{
		print_footer();
	}

	// Debug
	/// icalloc_mapPrint(); ifree_all(); icalloc_mapPrint();

	imain_end();
}

MBS
*sql_escape(
	MBS *pM
)
{
	MBS *p1 = ims_clone(pM);
	MBS *p2 = 0;
		p2 = ijs_replace(p1, ";", " "); // ";" => " " SQLインジェクション回避
	ifree(p1);
		p1 = p2;
		p2 = ijs_replace(p1, "*", "%"); // "*" => "%"
	ifree(p1);
		p1 = p2;
		p2 = ijs_replace(p1, "?", "_"); // "?" => "_"
	ifree(p1);
		p1 = p2;
		p2 = idate_replace_format_ymdhns(
			p1,
			"[", "]",
			"'",
			$aiNow[0],
			$aiNow[1],
			$aiNow[2],
			$aiNow[3],
			$aiNow[4],
			$aiNow[5]
		); // [] を日付に変換
	ifree(p1);
		p1 = p2;
	return p1;
}

/* 2016-08-19
	【留意】Dirの表示について
		d:\aaa\ 以下の、
			d:\aaa\bbb\
			d:\aaa\ccc\
			d:\aaa\ddd.txt
		を表示したときの違いは次のとおり。
		�@iwmls.exe（ls、dir互換）
			d:\aaa\bbb\
			d:\aaa\ccc\
			d:\aaa\ddd.txt
		�Aiwmfind.exe（BaseDirとFileを表示）
			d:\aaa\
			d:\aaa\ddd.txt
		※DirとFileを別テーブルで管理／joinして使用するため、このような仕様にならざるを得ない。
*/
VOID
ifind10(
	$struct_iFinfoW *FI,
	WCS *dir,
	UINT dirLenJ,
	INT depth
)
{
	if(depth > $iDepthMax)
	{
		return;
	}
	WIN32_FIND_DATAW F;
	WCS *wp1 = FI->fullnameW + iwi_cpy(FI->fullnameW, dir);
		*wp1 = L'*';
		*(++wp1) = 0;
	HANDLE hfind = FindFirstFileW(FI->fullnameW, &F);
		// 読み飛ばす Depth
		BOOL bMinDepthFlg = (depth >= $iDepthMin ? TRUE : FALSE);
		// dirIdを逐次生成
		UINT dirId = (++$uDirId);
		// Dir
		if(bMinDepthFlg)
		{
			ifind21(dir, dirId, depth);
			ifind22(FI, dirId, L"");
		}
		// File
		do
		{
			if(iFinfo_initW(FI, &F, dir, dirLenJ, F.cFileName))
			{
				if((FI->iFtype) == 1)
				{
					wp1 = iws_clone(FI->fullnameW);
						// 下位Dirへ
						ifind10(FI, wp1, FI->iEnd, depth + 1);
					ifree(wp1);
				}
				else if(bMinDepthFlg)
				{
					ifind22(FI, dirId, F.cFileName);
				}
			}
		}
		while(FindNextFileW(hfind, &F));
	FindClose(hfind);
	// 経過を表示
	ifind10_CallCnt(++$uCallCnt_ifind10);
}

VOID
ifind10_CallCnt(
	UINT uCnt
)
{
	if(uCnt >= 500 || ! uCnt)
	{
		PZ(9, NULL);
		fprintf(stderr, " > %u\r", $uAllCnt);
		PZ(-1, NULL);
		$uCallCnt_ifind10 = 0;
	}
}

VOID
ifind21(
	WCS *dir,
	UINT dirId,
	INT depth
)
{
	sqlite3_reset($stmt1);
		sqlite3_bind_int64($stmt1, 1, dirId);
		sqlite3_bind_text16($stmt1, 2, dir, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt1, 3, depth);
		sqlite3_bind_int($stmt1, 4, $uStepCnt);
	sqlite3_step($stmt1);
}

VOID
ifind22(
	$struct_iFinfoW *FI,
	UINT dirId,
	WCS *fname
)
{
	sqlite3_reset($stmt2);
		sqlite3_bind_int64($stmt2, 1, ++$uAllCnt);
		sqlite3_bind_int64($stmt2, 2, dirId);
		sqlite3_bind_text16($stmt2, 3, fname, -1, SQLITE_STATIC);
		sqlite3_bind_int($stmt2, 4, (FI->iExt) - (FI->iFname));
		sqlite3_bind_int($stmt2, 5, FI->iAttr);
		sqlite3_bind_double($stmt2, 6, FI->cjdCtime);
		sqlite3_bind_double($stmt2, 7, FI->cjdMtime);
		sqlite3_bind_double($stmt2, 8, FI->cjdAtime);
		sqlite3_bind_int64 ($stmt2, 9, FI->iFsize);
	sqlite3_step($stmt2);
}

VOID
sql_exec(
	sqlite3 *db,
	const MBS *sql,
	sqlite3_callback cb
)
{
	MBS *p_err = 0; // SQLiteが使用
	$uRowCnt = 0;
	$uBuf = 0;

	if(sqlite3_exec(db, sql, cb, 0, &p_err))
	{
		P("[Err] 構\文エラー\n    %s\n    %s\n", p_err, sql);
		sqlite3_free(p_err); // p_errを解放
		imain_end();
	}

	// sql_result_std() 対応
	if($uBuf)
	{
		P($sBuf);
	}
}

INT
sql_columnName(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	PZ(COLOR22, NULL);
		for($i1 = 0; $i1 < iColumnCount; $i1++)
		{
			MBS *p1 = U2M(sColumnNames[$i1]);
			P(
				"[%s]%s",
				p1,
				($i1 == iColumnCount - 1 ? "\n" : $sSeparate)
			);
			ifree(p1);
		}
	PZ(-1, NULL);
	return SQLITE_OK;
}

INT
sql_result_std(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	MBS *p1 = 0;
	INT iColumnCount2 = iColumnCount - 1;

	++$uRowCnt;

	for($i1 = 0; $i1 < iColumnCount; $i1++)
	{
		// [LN]
		if($i1 == $iSelectPosNumber)
		{
			$uBuf += sprintf(
				$sBuf + $uBuf,
				"%s%u%s%s",
				$sQuote,
				$uRowCnt,
				$sQuote,
				($i1 == iColumnCount2 ? "\n" : $sSeparate)
			);
		}
		else
		{
			p1 = U2M(sColumnValues[$i1]);
				$uBuf += sprintf(
					$sBuf + $uBuf,
					"%s%s%s%s",
					$sQuote,
					p1,
					$sQuote,
					($i1 == iColumnCount2 ? "\n" : $sSeparate)
				);
			ifree(p1);
		}
	}

	// Buf を Print
	if($uBuf > BUF_SIZE_MAX)
	{
		P($sBuf);
		$uBuf = 0;
	}

	return SQLITE_OK;
}

INT
sql_result_countOnly(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	++$uRowCnt;
	return SQLITE_OK;
}

INT
sql_result_exec(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	INT i1 = 0;
	MBS *p1 = 0, *p1e = 0, *p2 = 0, *p3 = 0, *p4 = 0, *p5 = 0;

	switch($iExec)
	{
		case(I_MKDIR): // --mkdir
		case(I_CP):    // --copy
		case(I_MV):    // --move
		case(I_MV2):   // --move2
			// $sMdOp\以下には、$aDirList\以下の dir を作成
			i1 = atoi(sColumnValues[0]); // step_cnt
			p1 = U2M(sColumnValues[1]);  // dir "\"付
			p1e = ijp_forwardN(p1, i1);  // p1のEOD
			p2 = U2M(sColumnValues[2]);  // name
			p3 = U2M(sColumnValues[3]);  // path
			// mkdir
			sprintf($sBuf, "%s\\%s", $sMdOp, p1e);
				if(imk_dir($sBuf))
				{
					P("md   => %s\n", $sBuf); // 新規dirを表示
					++$uRowCnt;
				}
			// 先
			p4 = ims_clone(p1e);
				sprintf($sBuf, "%s\\%s%s", $sMdOp, p4, p2);
			p5 = ims_clone($sBuf);
			if($iExec == I_CP)
			{
				if(CopyFile(p3, p5, FALSE))
				{
					P("cp   <= %s\n", p3); // fileを表示
					++$uRowCnt;
				}
			}
			else if($iExec >= I_MV)
			{
				// ReadOnly属性(1)を解除
				if((1 & GetFileAttributes(p5)))
				{
					SetFileAttributes(p5, FALSE);
				}
				// 既存file削除
				if(iFchk_typePathA(p5))
				{
					irm_file(p5);
				}
				if(MoveFile(p3, p5))
				{
					P("mv   <= %s\n", p3); // fileを表示
					++$uRowCnt;
				}
				// rmdir
				if($iExec == I_MV2)
				{
					if(irm_dir(p1))
					{
						P("rd   => %s\n", p1); // dirを表示
						++$uRowCnt;
					}
				}
			}
			ifree(p5);
			ifree(p4);
			ifree(p3);
			ifree(p2);
			ifree(p1);
		break;

		case(I_EXT1): // --extract1
		case(I_EXT2): // --extract2
			p1 = U2M(sColumnValues[0]); // path
			p2 = U2M(sColumnValues[1]); // name
				// mkdir
				if(imk_dir($sMdOp))
				{
					P("md   => %s\n", $sMdOp); // 新規dirを表示
					++$uRowCnt;
				}
				// 先
				sprintf($sBuf, "%s\\%s", $sMdOp, p2);
				p3 = $sBuf;
					// I_EXT1, I_EXT2共、同名fileは上書き
					if($iExec == I_EXT1)
					{
						if(CopyFile(p1, p3, FALSE))
						{
							P("cp   <= %s\n", p3); // fileを表示

							++$uRowCnt;
						}
					}
					else if($iExec == I_EXT2)
					{
						// ReadOnly属性(1)を解除
						if((1 & GetFileAttributes(p3)))
						{
							SetFileAttributes(p3, FALSE);
						}
						// dir存在していれば削除しておく
						if(iFchk_typePathA(p3))
						{
							irm_file(p3);
						}
						if(MoveFile(p1, p3))
						{
							P("mv   <= %s\n", p3); // fileを表示
							++$uRowCnt;
						}
					}
			ifree(p2);
			ifree(p1);
		break;

		case(I_RP): // --replace
			p1 = U2M(sColumnValues[0]); // type
			p2 = U2M(sColumnValues[1]); // path
				if(*p1 == 'f' && CopyFile($sRepOp, p2, FALSE))
				{
					P("rep  => %s\n", p2); // fileを表示
					++$uRowCnt;
				}
			ifree(p2);
			ifree(p1);
		break;

		case(I_RM):  // --remove
		case(I_RM2): // --remove2
			p1 = U2M(sColumnValues[0]); // path
			p2 = U2M(sColumnValues[1]); // dir
			// ReadOnly属性(1)を解除
			if((1 & atoi(sColumnValues[2])))
			{
				SetFileAttributes(p1, FALSE);
			}
			// file 削除
			if(irm_file(p1))
			{
				// fileのみ
				P("rm   => %s\n", p1); // fileを表示
				++$uRowCnt;
			}
			// 空dir 削除
			if($iExec == I_RM2)
			{
				// 空dirである
				if(irm_dir(p2))
				{
					P("rd   => %s\n", p2); // dirを表示
					++$uRowCnt;
				}
			}
			ifree(p2);
			ifree(p1);
		break;
	}
	return SQLITE_OK;
}

BOOL
sql_saveOrLoadMemdb(
	sqlite3 *mem_db, // ":memory:"
	MBS *ofn,        // filename
	BOOL save_load   // TRUE=save, FALSE=load
)
{
	INT err = 0;
	sqlite3 *pFile;
	sqlite3_backup *pBackup;
	sqlite3 *pTo;
	sqlite3 *pFrom;

	// UTF-8で処理
	U8N *up1 = M2U(ofn);
		if(! (err = sqlite3_open(up1, &pFile)))
		{
			if(save_load)
			{
				pFrom = mem_db;
				pTo =pFile;
			}
			else
			{
				pFrom = pFile;
				pTo = mem_db;
			}
			pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
			if(pBackup)
			{
				sqlite3_backup_step(pBackup, -1);
				sqlite3_backup_finish(pBackup);
			}
			err = sqlite3_errcode(pTo);
		}
		sqlite3_close(pFile);
	ifree(up1);

	return (! err ? TRUE : FALSE);
}

VOID
print_footer()
{
	MBS *p1 = 0;
	PZ(COLOR22, NULL);
		LN();
		P(
			"-- %u row%s in set ( %.3f sec)\n",
			$uRowCnt,
			($uRowCnt > 1 ? "s" : ""), // 複数形
			iExecSec_next()
		);
	PZ(COLOR12, NULL);
		P2("--");
		for(UINT _u1 = 0; _u1 < $aDirListSize; _u1++)
		{
			P("--  $aDirList(%u) \"%s\"\n", _u1 + 1, $aDirList[_u1]);
		}
		p1 = U2M($sqlU);
			P("--  $sqlU        \"%s\"\n", p1);
		ifree(p1);
		if(*$sInDbn)
		{
			P("--  -in          \"%s\"\n", $sInDbn);
		}
		if(*$sOutDbn)
		{
			P("--  -out         \"%s\"\n", $sOutDbn);
		}
		if($bNoFooter)
		{
			P("--  -nofooter\n");
		}
		if(*$sQuote)
		{
			P("--  -quote       \"%s\"\n", $sQuote);
		}
		if(*$sSeparate)
		{
			P("--  -separate    \"%s\"\n", $sSeparate);
		}
		P2("--");
	PZ(-1, NULL);
}

VOID
print_version()
{
	PZ(COLOR92, NULL);
	LN();
	P(" %s\n", IWM_COPYRIGHT);
	P("   Ver.%s+%s+SQLite%s\n", IWM_VERSION, LIB_IWMUTIL_VERSION, SQLITE_VERSION);
	LN();
	PZ(-1, NULL);
}

VOID
print_help()
{
	print_version();
	PZ(COLOR01, " ファイル検索 \n\n");
	PZ(COLOR11, " %s [Dir] [Option] \n\n", $IWM_CMD);
	PZ(COLOR12, " (例１) ");
	PZ(COLOR91, "検索\n");
	P("   %s DIR -r -s=LN,path,size -w=\"ext like 'exe'\"\n\n", $IWM_CMD);
	PZ(COLOR12, " (例２) ");
	PZ(COLOR91, "検索結果をファイルへ保存\n");
	P("   %s DIR1 DIR2 ... -r -o=FILE [Option]\n\n", $IWM_CMD);
	PZ(COLOR12, " (例３) ");
	PZ(COLOR91, "検索対象をファイルから読込\n");
	P("   %s -i=FILE [Option]\n\n", $IWM_CMD);
	PZ(COLOR21, " [Dir]\n");
	PZ(COLOR91, "   検索対象 dir\n");
	PZ(COLOR12, "   (例) ");
	PZ(COLOR91, "\"c:\\\" \"c:\\windows\\\" => \"c:\\\"\n");
	PZ(COLOR13, "   (注) ");
	PZ(COLOR91, "複数指定の場合、上位Dirに集約する\n\n");
	PZ(COLOR21, " [Option]\n");
	PZ(COLOR22, "   -out=FILE | -o=FILE\n");
	PZ(COLOR91, "       出力ファイル\n\n");
	PZ(COLOR22, "   -in=FILE | -i=FILE\n");
	PZ(COLOR91, "       入力ファイル\n");
	PZ(COLOR13, "       (注) ");
	PZ(COLOR91, "検索対象 dir と併用できない\n\n");
	PZ(COLOR22, "   -select=COLUMN1,COLUMN2,... | -s=COLUMN1,COLUMN2,...\n");
	PZ(COLOR91, NULL);
	P2("       LN        (NUM) 表\示順");
	P2("       path      (STR) dir\\name");
	P2("       dir       (STR) ディレクトリ名");
	P2("       name      (STR) ファイル名");
	P2("       ext       (STR) 拡張子");
	P2("       depth     (NUM) ディレクトリ階層 = 0..");
	P2("       type      (STR) ディレクトリ = d／ファイル = f");
	P2("       attr_num  (NUM) 属性");
	P2("       attr      (STR) 属性 \"[d|f][r|-][h|-][s|-][a|-]\"");
	P2("                       [dir|file][read-only][hidden][system][archive]");
	P2("       size      (NUM) ファイルサイズ = byte");
	P2("       ctime_cjd (NUM) 作成日時     -4712/01/01 00:00:00始点の通算日／CJD=JD-0.5");
	P2("       ctime     (STR) 作成日時     \"yyyy-mm-dd hh:nn:ss\"");
	P2("       mtime_cjd (NUM) 更新日時     ctime_cjd参照");
	P2("       mtime     (STR) 更新日時     ctime参照");
	P2("       atime_cjd (NUM) アクセス日時 ctime_cjd参照");
	P2("       atime     (STR) アクセス日時 ctime参照\n");
	P2("       ※１ COLUMN指定なしの場合");
	P("            %s 順で表\示\n\n", OP_SELECT_1);
	P2("       ※２ SQLite演算子／関数を利用可能\");
	PZ(COLOR12, "            (例)\n");
	PZ(COLOR91, NULL);
	P2("              abs(X)  changes()  char(X1, X2, ..., XN)  coalesce(X, Y, ...)");
	P2("              glob(X, Y)  ifnull(X, Y)  instr(X, Y)  hex(X)");
	P2("              last_insert_rowid()  length(X)");
	P2("              like(X, Y)  like(X, Y, Z)  likelihood(X, Y)  likely(X)");
	P2("              load_extension(X)  load_extension(X, Y)  lower(X)");
	P2("              ltrim(X)  ltrim(X, Y)  max(X, Y, ...)  min(X, Y, ...)");
	P2("              nullif(X, Y)  printf(FORMAT, ...)  quote(X)");
	P2("              random()  randomblob(N)  replace(X, Y, Z)");
	P2("              round(X)  round(X, Y)  rtrim(X)  rtrim(X, Y)");
	P2("              soundex(X)  sqlite_compileoption_get(N)");
	P2("              sqlite_compileoption_used(X)  sqlite_source_id()");
	P2("              sqlite_version()  substr(X, Y, Z) substr(X, Y)");
	P2("              total_changes()  trim(X) trim(X, Y)  typeof(X)  unlikely(X)");
	P2("              unicode(X)  upper(X)  zeroblob(N)");
	PZ(COLOR13, "           (注) ");
	PZ(COLOR91, "マルチバイト文字を１文字として処理\n");
	P2("           (参考) http://www.sqlite.org/lang_corefunc.html\n");
	PZ(COLOR22, "   -where=STR | -w=STR\n");
	PZ(COLOR12, "       (例１) ");
	PZ(COLOR91, "\"size<=100 or size>1000000\"\n");
	PZ(COLOR12, "       (例２) ");
	PZ(COLOR91, "\"type like 'f' and name like 'abc??.*'\"\n");
	P2("              '?' '_' は任意の1文字");
	P2("              '*' '%' は任意の0文字以上");
	PZ(COLOR12, "       (例３) ");
	PZ(COLOR91, "基準日 \"2010-12-10 12:00:00\"のとき\n");
	P2("              \"ctime>[-10D]\"   => \"ctime>'2010-11-30 00:00:00'\"");
	P2("              \"ctime>[-10d]\"   => \"ctime>'2010-11-30 12:00:00'\"");
	P2("              \"ctime>[-10d*]\"  => \"ctime>'2010-11-30 %'\"");
	P2("              \"ctime>[-10d%]\" => \"ctime>'2010-11-30 %'\"");
	P2("              (年) Y, y (月) M, m (日) D, d (時) H, h (分) N, n (秒) S, s\n");
	PZ(COLOR22, "   -group=STR | -g=STR\n");
	PZ(COLOR12, "       (例) ");
	PZ(COLOR91, "-g=\"STR1, STR2\"\n");
	P2("            STR1とSTR2をグループ毎にまとめる\n");
	PZ(COLOR22, "   -sort=\"STR [ASC|DESC]\" | -st=\"STR [ASC|DESC]\"\n");
	PZ(COLOR12, "       (例) ");
	PZ(COLOR91, "-st=\"STR1 ASC, STR2 DESC\"\n");
	P2("            STR1を順ソ\ート, STR2を逆順ソ\ート\n");
	PZ(COLOR22, "   -recursive | -r\n");
	PZ(COLOR91, "       下階層を全検索\n\n");
	PZ(COLOR22, "   -depth=NUM1,NUM2 | -d=NUM1,NUM2\n");
	PZ(COLOR91, "       検索する下階層を指定\n");
	PZ(COLOR12, "       (例１) ");
	PZ(COLOR91, "-d=\"1\"\n");
	P2("              1階層のみ検索");
	PZ(COLOR12, "       (例２) ");
	PZ(COLOR91, "-d=\"3\",\"5\"\n");
	P2("              3〜5階層を検索\n");
	P2("       ※１ CurrentDir は \"0\"");
	P2("       ※２ -depth と -where における depth の挙動の違い");
	P2("            ◇(速い) -depth は指定された階層のみ検索を行う");
	P2("            ◇(遅い) -where内でのdepthによる検索は全階層のdir／fileに対して行う\n");
	PZ(COLOR22, "   -noheader | -nh\n");
	PZ(COLOR91, "       ヘッダ情報を表\示しない\n\n");
	PZ(COLOR22, "   -nofooter | -nf\n");
	PZ(COLOR91, "       フッタ情報を表\示しない\n\n");
	PZ(COLOR22, "   -quote=STR | -qt=STR\n");
	PZ(COLOR91, "       囲み文字\n");
	PZ(COLOR12, "       (例) ");
	PZ(COLOR91, "-qt=\"'\"\n\n");
	PZ(COLOR22, "   -separate=STR | -sp=STR\n");
	PZ(COLOR91, "       区切り文字\n");
	PZ(COLOR12, "       (例) ");
	PZ(COLOR91, "-sp=\"\\t\"\n\n");
	PZ(COLOR22, "   --mkdir=DIR | --md=DIR\n");
	PZ(COLOR91, "       検索結果からdirを抽出し DIR に作成する(階層維持)\n\n");
	PZ(COLOR22, "   --copy=DIR | --cp=DIR\n");
	PZ(COLOR91, "       --mkdir + 検索結果を DIR にコピーする(階層維持)\n\n");
	PZ(COLOR22, "   --move=DIR | --mv=DIR\n");
	PZ(COLOR91, "       --mkdir + 検索結果を DIR に移動する(階層維持)\n\n");
	PZ(COLOR22, "   --move2=DIR | --mv2=DIR\n");
	PZ(COLOR91, "       --mkdir + --move + 移動元の空dirを削除する(階層維持)\n\n");
	PZ(COLOR22, "   --extract1=DIR | --ext1=DIR\n");
	PZ(COLOR91, "       --mkdir + 検索結果ファイルのみ抽出し DIR にコピーする\n");
	PZ(COLOR13, "       (注) ");
	PZ(COLOR91, "階層を維持しない／同名ファイルは上書き\n\n");
	PZ(COLOR22, "   --extract2=DIR | --ext2=DIR\n");
	PZ(COLOR91, "       --mkdir + 検索結果ファイルのみ抽出し DIR に移動する\n");
	PZ(COLOR13, "       (注) ");
	PZ(COLOR91, "階層を維持しない／同名ファイルは上書き\n\n");
	PZ(COLOR22, "   --remove | --rm\n");
	PZ(COLOR91, "       検索結果のfileのみ削除する（dirは削除しない）\n\n");
	PZ(COLOR22, "   --remove2 | --rm2\n");
	PZ(COLOR91, "       --remove + 空dirを削除する\n\n");
	PZ(COLOR22, "   --replace=FILE | --rep=FILE\n");
	PZ(COLOR91, "       検索結果(複数) を FILE の内容で置換(上書き)する／ファイル名は変更しない\n");
	PZ(COLOR12, "       (例) ");
	PZ(COLOR91, "-w=\"name like 'foo.txt'\" --rep=\".\\foo.txt\"\n\n");
	PZ(COLOR92, NULL);
	LN();
	PZ(-1, NULL);
}
