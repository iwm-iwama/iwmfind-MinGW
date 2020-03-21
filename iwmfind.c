//----------------------------------------------------------------
#define   IWMFIND_VERSION     "iwmfind3_20200223"
#define   IWMFIND_COPYRIGHT   "Copyright (C)2009-2020 iwm-iwama"
//----------------------------------------------------------------
#include  "lib_iwmutil.h"
#include  "sqlite3.h"
MBS  *str_encode(MBS *pM);
MBS  *sql_escape(MBS *pM);
VOID ifind10($struct_iFinfoW *FI, WCS *dir, UINT dirLenJ, INT depth);
VOID ifind21(WCS *dir, UINT dirId, INT depth);
VOID ifind22($struct_iFinfoW *FI, UINT dirId, WCS *fname);
VOID sql_exec(sqlite3 *db, const MBS *sql, sqlite3_callback cb);
INT  print_result(VOID *option, INT iColumnCount, MBS **sColumnValues, MBS **sColumnNames);
BOOL sql_saveOrLoadMemdb(sqlite3 *mem_db, MBS *ofn, BOOL save_load);
VOID print_footer();
VOID print_version();
VOID print_help();
//-----------
// ���p�ϐ�
//-----------
UINT $execMS = 0;
MBS  *$program = 0;
MBS  *$sTmp = 0;    // ��� icalloc_MBS(IGET_ARGS_LEN_MAX)
U8N  *$sqlU = 0;
UINT $uDirId = 0;   // Dir��
UINT $uAllCnt = 0;  // ������
UINT $uStepCnt = 0; // Currentdir�ʒu
UINT $uRowCnt = 0;  // �����s��
sqlite3      *$iDbs = 0;
sqlite3_stmt *$stmt1 = 0, *$stmt2 = 0;
// [�����F] + ([�w�i�F] * 16)
//  0 = Black    1 = Navy     2 = Green    3 = Teal
//  4 = Maroon   5 = Purple   6 = Olive    7 = Silver
//  8 = Gray     9 = Blue    10 = Lime    11 = Aqua
// 12 = Red     13 = Fuchsia 14 = Yellow  15 = White
#define   ColorHeaderFooter   (10 + ( 0 * 16))
#define   ColorExp1           (12 + ( 0 * 16))
#define   ColorExp2           (14 + ( 0 * 16))
#define   ColorExp3           (11 + ( 0 * 16))
#define   ColorText1          (15 + ( 0 * 16))
#define   ColorText2          ( 7 + ( 0 * 16))
#define   ColorBgText1        (15 + (12 * 16))

UINT _getColorDefault = 0;
#define   MEMDB     ":memory:"
#define   OLDDB     ("iwmfind.db."IWMFIND_VERSION)
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
			"CREATE VIEW V_INDEX AS SELECT T_FILE.id, \
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
				T_FILE.number AS number, \
				T_FILE.flg AS flg, \
				depth, \
				step_byte \
				FROM T_FILE LEFT JOIN T_DIR ON T_FILE.dir_id=T_DIR.dir_id;"
#define   UPDATE_EXEC99_1 \
			"UPDATE T_FILE SET flg=1 WHERE id IN (SELECT id FROM V_INDEX %s);"
#define   DELETE_EXEC99_1 \
			"DELETE FROM T_FILE WHERE flg IS NULL;"
#define   UPDATE_EXEC99_2 \
			"UPDATE T_FILE SET flg=NULL;"
#define   SELECT_VIEW \
			"SELECT %s FROM V_INDEX %s %s %s ORDER BY %s;"
#define   OP_SELECT_0 \
			"number, path"
#define   OP_SELECT_1 \
			"number, path, depth, type, size, ctime, mtime, atime"
#define   OP_SELECT_MKDIR \
			"step_byte, dir, name, path"
#define   OP_SELECT_EXTRACT \
			"path, name"
#define   OP_SELECT_RP \
			"type, path"
#define   OP_SELECT_RM \
			"path, dir, attr_num"
#define   I_MKDIR    1
#define   I_CP       2
#define   I_MV       3
#define   I_MV2      4
#define   I_EXT1     5
#define   I_EXT2     6
#define   I_RP      11
#define   I_RM      21
#define   I_RM2     22
/*
	���ݎ���
*/
INT *_aiNow = 0;
/*
	����dir
*/
MBS **_aDir = 0;
INT _aDirNum = 0;
/*
	����DB
	-in | -i FileName
*/
MBS *_sIn = 0;
MBS *_sInDbn = 0;
/*
	�o��DB
	-out | -o FileName
*/
MBS *_sOut = 0;
MBS *_sOutDbn = 0;
/*
	select ��
	-select | -s ColumnName
*/
MBS *_sSelect = 0;
INT _iSelectPosNum = 0; // "number"�̔z��ʒu
/*
	where ��
	-where | -w STR
*/
// xMBS *_where = 0;
MBS *_sWhere0 = 0;
MBS *_sWhere1 = 0;
MBS *_sWhere2 = 0;
/*
	group by ��
	-group | -g ColumnName
*/
MBS *_sGroup = 0;
/*
	order by ��
	-sort | -st ColumnName
*/
MBS *_sSort = 0;
/*
	�t�b�^�����\��
	-noguide | -ng
*/
BOOL _bNoGuide = FALSE;
/*
	�o�͂� STR �ň͂�
	-quote | -qt STR
*/
MBS *_sQuote = 0;
/*
	�o�͂̑O��ɃR�����g��t��
	-table STR1 STR2
*/
MBS *_sTable1 = 0;
MBS *_sTable2 = 0;
/*
	�o�͍s�̑O��ɃR�����g��t��
	-tr STR1 STR2
*/
MBS *_sTr1 = 0;
MBS *_sTr2 = 0;
/*
	�o�͂� STR �ŋ�؂�
	-td STR1 STR2
*/
MBS *_sTd1 = 0;
MBS *_sTd2 = 0;
/*
	HTML Table�o��
		*_sTable1 = "<table border=\"1\">"
		*_sTable2 = "</table>"
		*_sTr1    = "<tr>"
		*_sTr2    = "</tr>"
		*_sTd1    = "<td>"
		*_sTd2    = "</td>"
	�Ɠ��`
*/
BOOL _bHtml = FALSE;
/*
	����Dir�ʒu
	-depth | -d NUM1 NUM2
*/
INT _iMinDepth = 0; // ���K�w�`�J�n�ʒu(����)
INT _iMaxDepth = 0; // ���K�w�`�I���ʒu(����)
/*
	--mkdir | --md DIR
	mkdir DIR
*/
MBS *_sMd = 0;
MBS *_sOpMd = 0;
/*
	--copy | --cp DIR
	mkdir DIR & copy
*/
MBS *_sCp = 0;
/*
	--move | --mv DIR
	mkdir DIR & move DIR
*/
MBS *_sMv = 0;
/*
	--move2 | --mv2 DIR
	mkdir DIR & move DIR & rmdir
*/
MBS *_sMv2 = 0;
/*
	--extract1 | --ext1 DIR
	copy FileOnly
*/
MBS *_sExt1 = 0;
/*
	--extract2 | --ext2 DIR
	move FileOnly
*/
MBS *_sExt2 = 0;
/*
	--replace | --rep FILE
	replace results with FILE
*/
MBS *_sRep = 0;
MBS *_sOpRep = 0;
/*
	--rm  | --remove
	--rm2 | --remove2
*/
INT _iRm = 0;
/*
	_iExec
	�D�揇 (���S)I_MD > (�댯)I_RM2
		I_MD  = --mkdir
		I_CP  = --cp
		I_MV  = --mv
		I_MV2 = --mv2
		I_RP  = --rep
		I_RM  = --rm
		I_RM2 = --rm2
*/
INT _iExec = 0;
/*
	_iPriority
	���s�D�揇
		1 = �D��
		2 = �ʏ�ȏ�
		3 = �ʏ�(default)
		4 = �ʏ�ȉ�
		5 = �A�C�h����
*/
#define   PRIORITY_DEFAULT    3
INT _iPriority = PRIORITY_DEFAULT;

INT
main()
{
	// ���s����
	$execMS = iExecSec_init();
	// ���ݎ���
	_aiNow = (INT*)idate_cjd_to_iAryYmdhns(idate_nowToCjd(TRUE));
	// ���ϐ�
	$sTmp = icalloc_MBS(IGET_ARGS_LEN_MAX + 1); // +"\0"
	_getColorDefault = iConsole_getBgcolor();   // ���݂̕����F�^�w�i�F
	// �ϐ�
	$program = iCmdline_getCmd();
	MBS **args = iCmdline_getArgs();
	INT argc = iargs_size(args);
	MBS **ap1 = {0}, **ap2 = {0}, **ap3 = {0};
	MBS *p1 = 0, *p2 = 0;
	INT i1 = 0, i2 = 0;
	// -help | -h
	ap1 = iargs_option(args, "-help", "-h");
		if($IWM_bSuccess || !**args)
		{
			print_help();
			imain_end();
		}
	ifree(ap1);
	// -version | -v
	ap1 = iargs_option(args, "-version", "-v");
		if($IWM_bSuccess)
		{
			print_version();
			LN();
			imain_end();
		}
	ifree(ap1);
	/*
		_aDir�擾�� _iMaxDepth ���g�����ߐ�ɁA
			-recursive
			-depth
		���`�F�b�N
	*/
	// -recursive | -r
	ap1 = iargs_option(args, "-recursive", "-r");
		if($IWM_bSuccess)
		{
			_iMinDepth = 0;
			_iMaxDepth = ($IWM_uAryUsed ? inum_atoi(*(ap1 + 0)) : IMAX_PATHA);
		}
	ifree(ap1);
	// -depth | -d
	ap1 = iargs_option(args, "-depth", "-d");
		if($IWM_bSuccess)
		{
			if($IWM_uAryUsed > 1)
			{
				_iMinDepth = inum_atoi(*(ap1 + 0));
				_iMaxDepth = inum_atoi(*(ap1 + 1));
			}
			else if($IWM_uAryUsed == 1)
			{
				_iMinDepth = _iMaxDepth = inum_atoi(*(ap1 + 0));
			}
			else
			{
				_iMinDepth = _iMaxDepth = 0;
			}
		}
	ifree(ap1);
	/*
		_iMinDepth
		_iMaxDepth
		�ŏI�`�F�b�N
	*/
	// swap
	if(_iMinDepth > _iMaxDepth)
	{
		i1 = _iMinDepth;
		_iMinDepth = _iMaxDepth;
		_iMaxDepth = i1;
	}
	// _aDir ���쐬
	for(i1 = 0, i2 = 0; i1 < argc; i1++)
	{
		p1 = *(args + i1);
		if(*p1 == '-')
		{
			break;
		}
		// dir�s��
		if(iFchk_typePathA(p1) != 1)
		{
			fprintf(stderr, "[Err] Dir(%d) '%s' �͑��݂��Ȃ�!\n", (i1 + 1), p1);
			++i2;
		}
	}
	if(i2)
	{
		imain_end(); // �������~
	}
	ap1 = icalloc_MBS_ary(i1);
		for(i1 = 0; i1 < argc; i1++)
		{
			p1 = *(args + i1);
			// ��r�� null=>������ ��r�łȂ��ƃG���[
			if(!p1 || *p1 == '-')
			{
				break;
			}
			if(iFchk_typePathA(p1) == 1)
			{
				*(ap1 + i1) = iFget_AdirA(p1); // ���PATH�֕ϊ�
			}
		}
		// ���Dir�̂ݎ擾
		_aDir = iary_higherDir(ap1, _iMaxDepth);
		_aDirNum = $IWM_uAryUsed;
	ifree(ap1);
	// -in | -i
	ap1 = iargs_option(args, "-in", "-i");
		_sIn = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
		if(*_sIn)
		{
			// DB�s��
			if(iFchk_typePathA(_sIn) != 2)
			{
				fprintf(stderr, "[Err] -in '%s' �͑��݂��Ȃ�!\n", _sIn);
				imain_end();
			}
			if(_aDirNum)
			{
				fprintf(stderr, "[Err] Dir �� -in �͕��p�ł��Ȃ�!\n");
				imain_end();
			}
			_sInDbn = ims_clone(_sIn);
			// -in �̂Ƃ��� -recursive �����t�^
			_iMinDepth = 0;
			_iMaxDepth = IMAX_PATHA;
		}
		else
		{
			_sInDbn = MEMDB;
		}
	ifree(ap1);
	// -out | -o
	ap1 = iargs_option(args, "-out", "-o");
		_sOut = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
		if(*_sOut)
		{
			// Dir or inDb
			if(!*_sIn && !_aDirNum)
			{
				fprintf(stderr, "[Err] Dir �������� -in ���w�肵�Ă�������!\n");
				imain_end();
			}
			_sOutDbn = ims_clone(_sOut);
		}
		else
		{
			_sOutDbn = "";
		}
	ifree(ap1);
	// Err����
	if(!*_sIn && !*_sOut && !_aDirNum)
	{
		print_help();
		imain_end();
	}
	// --mkdir | --md
	ap1 = iargs_option(args, "--mkdir", "--md");
		_sMd = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --copy | --cp
	ap1 = iargs_option(args, "--copy", "--cp");
		_sCp = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --move | --mv
	ap1 = iargs_option(args, "--move", "--mv");
		_sMv = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --move2 | --mv2
	ap1 = iargs_option(args, "--move2", "--mv2");
		_sMv2 = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --extract1 | --ext1
	ap1 = iargs_option(args, "--extract1", "--ext1");
		_sExt1 = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --extract2 | --ext2
	ap1 = iargs_option(args, "--extract2", "--ext2");
		_sExt2 = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --replace | --rep
	ap1 = iargs_option(args, "--replace", "--rep");
		_sRep = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
	ifree(ap1);
	// --remove | --rm
	ap1 = iargs_option(args, "--remove", "--rm");
		if($IWM_bSuccess)
		{
			_iRm = I_RM;
		}
	ifree(ap1);
	// --remove2 | --rm2
	ap1 = iargs_option(args, "--remove2", "--rm2");
		if($IWM_bSuccess)
		{
			_iRm = I_RM2;
		}
	ifree(ap1);
	// --[exec]
	if(*_sMd || *_sCp || *_sMv || *_sMv2 || *_sExt1 || *_sExt2 || *_sRep || _iRm)
	{
		if(*_sMd)
		{
			_iExec = I_MKDIR;
			_sOpMd = ims_clone(_sMd);
		}
		else if(*_sCp)
		{
			_iExec = I_CP;
			_sOpMd = ims_clone(_sCp);
		}
		else if(*_sMv)
		{
			_iExec = I_MV;
			_sOpMd = ims_clone(_sMv);
		}
		else if(*_sMv2)
		{
			_iExec = I_MV2;
			_sOpMd = ims_clone(_sMv2);
		}
		else if(*_sExt1)
		{
			_iExec = I_EXT1;
			_sOpMd = ims_clone(_sExt1);
		}
		else if(*_sExt2)
		{
			_iExec = I_EXT2;
			_sOpMd = ims_clone(_sExt2);
		}
		else if(*_sRep)
		{
			if(iFchk_typePathA(_sRep) != 2)
			{
				fprintf(stderr, "[Err] --replace '%s' �͑��݂��Ȃ�!\n", _sRep);
				imain_end();
			}
			else
			{
				_iExec = I_RP;
				_sOpRep = ims_clone(_sRep);
			}
		}
		else if(_iRm)
		{
			_iExec = _iRm;
		}

		_bNoGuide = TRUE; // �t�b�^�����\��

		if(_iExec <= I_EXT2)
		{
			p1 = iFget_AdirA(_sOpMd);
				_sOpMd = ijs_cutR(p1, "\\");
			ifree(p1);
			_sSelect = (_iExec <= I_MV2 ? OP_SELECT_MKDIR : OP_SELECT_EXTRACT);
		}
		else if(_iExec == I_RP)
		{
			_sSelect = OP_SELECT_RP;
		}
		else
		{
			_sSelect = OP_SELECT_RM;
		}
	}
	else
	{
		_sOpMd = "";
		/*
			(none)         : OP_SELECT_0
			-select (none) : OP_SELECT_1
			-select COLUMN1, COLUMN2, ...
		*/
		// -select | -s
		ap1 = iargs_option(args, "-select", "-s");
			if($IWM_bSuccess)
			{
				if($IWM_uAryUsed)
				{
					// "number"�ʒu�����߂�
					ap2 = ija_token(*(ap1 + 0), ", ");
						ap3 = iary_simplify(ap2, TRUE); // number�\���͂P�����o���Ȃ��̂ŏd���r��
							_iSelectPosNum = 0;
							while((p2 = *(ap3 + _iSelectPosNum)))
							{
								if(imb_cmppi(p2, "number"))
								{
									break;
								}
								++_iSelectPosNum;
							}
							if(!p2)
							{
								_iSelectPosNum = -1;
							}
							_sSelect = iary_toA(ap3, ", ");
						ifree(ap3);
					ifree(ap2);
				}
				else
				{
					_sSelect = OP_SELECT_1;
				}
			}
			else
			{
				_sSelect = OP_SELECT_0;
			}
		ifree(ap1);
		// -sort
		ap1 = iargs_option(args, "-sort", "-st");
			_sSort = ($IWM_uAryUsed ? ims_clone(*(ap1 + 0)) : "");
		ifree(ap1);
	}
	/*
		_sSort => SORT�֌W���ꊇ�ϊ�
	*/
	if(_iExec >= I_MV)
	{
		// Dir�폜���K�v�Ȃ��̂� "order by desc"
		ifree(_sSort);
		_sSort = "path desc";
	}
	else
	{
		if(!_sSort || !*_sSort)
		{
			ifree(_sSort);
			_sSort = "dir, name";
		}
		/*
			path, dir, name, ext
			�\�[�g�́A�啶���E����������ʂ��Ȃ�
		*/
		p1 = ims_clone(_sSort);
			p2 = ijs_replace(p1, "path", "lower(path)");
				ifree(p1);
				p1 = p2;
			p2 = ijs_replace(p1, "dir", "lower(dir)");
				ifree(p1);
				p1 = p2;
			p2 = ijs_replace(p1, "name", "lower(name)");
				ifree(p1);
				p1 = p2;
			p2 = ijs_replace(p1, "ext", "lower(ext)");
				ifree(p1);
				p1 = p2;
			_sSort = ims_clone(p1);
		ifree(p1);
	}
	// -where | -w
	snprintf($sTmp, IGET_ARGS_LEN_MAX, "depth>=%d AND depth<=%d", _iMinDepth, _iMaxDepth);
		_sWhere2 = ims_clone($sTmp);
	ap1 = iargs_option(args, "-where", "-w");
		if($IWM_uAryUsed)
		{
			_sWhere0 = sql_escape(*(ap1 + 0));
			snprintf($sTmp, IGET_ARGS_LEN_MAX, "WHERE %s AND", _sWhere0);
			_sWhere1 = ims_clone($sTmp);
		}
		else
		{
			_sWhere0 = "";
			_sWhere1 = "WHERE";
		}
	ifree(ap1);
	// -group | -g
	ap1 = iargs_option(args, "-group", "-g");
		if($IWM_uAryUsed)
		{
			snprintf($sTmp, IGET_ARGS_LEN_MAX, "GROUP BY %s", *(ap1 + 0));
			_sGroup = ims_clone($sTmp);
		}
		else
		{
			_sGroup = "";
		}
	ifree(ap1);
	// -noguide | -ng
	ap1 = iargs_option(args, "-noguide", "-ng");
		_bNoGuide = $IWM_bSuccess;
	ifree(ap1);
	// -quote | -qt
	ap1 = iargs_option(args, "-quote", "-qt");
		_sQuote = ($IWM_uAryUsed ? str_encode(*(ap1 + 0)) : "");
	ifree(ap1);
	// -table
	ap1 = iargs_option(args, "-table", NULL);
		_sTable1 = ($IWM_uAryUsed > 0 ? str_encode(*(ap1 + 0)) : "");
		_sTable2 = ($IWM_uAryUsed > 1 ? str_encode(*(ap1 + 1)) : "");
	ifree(ap1);
	// -tr
	ap1 = iargs_option(args, "-tr", NULL);
		_sTr1 = ($IWM_uAryUsed > 0 ? str_encode(*(ap1 + 0)) : "");
		_sTr2 = ($IWM_uAryUsed > 1 ? str_encode(*(ap1 + 1)) : "");
	ifree(ap1);
	// -td
	ap1 = iargs_option(args, "-td", NULL);
		if($IWM_uAryUsed > 1)
		{
			_sTd1 = str_encode(*(ap1 + 0));
			_sTd2 = str_encode(*(ap1 + 1));
		}
		else if($IWM_uAryUsed == 1)
		{
			_sTd1 = "";
			_sTd2 = str_encode(*(ap1 + 0));
		}
		else
		{
			_sTd1 = "";
			_sTd2 = " | ";
		}
	ifree(ap1);
	// -html
	ap1 = iargs_option(args, "-html", NULL);
		_bHtml = $IWM_bSuccess;
		if(_bHtml)
		{
			// free
			ifree(_sTable1);
			ifree(_sTable2);
			ifree(_sTr1);
			ifree(_sTr2);
			ifree(_sTd1);
			ifree(_sTd2);
			// �Ē�`
			_sTable1 = "<table border=\"1\">";
			_sTable2 = "</table>";
			_sTr1    = "<tr>";
			_sTr2    = "</tr>";
			_sTd1    = "<td>";
			_sTd2    = "</td>";
		}
	ifree(ap1);
	// ---priority | ---pr
	ap1 = iargs_option(args, "---priority", "---pr");
		i1 = ($IWM_uAryUsed ? inum_atoi(*(ap1 + 0)) : 0);
		if(i1 < 1 || i1 > 5)
		{
			i1 = PRIORITY_DEFAULT; // default=3
		}
		_iPriority = i1;
	ifree(ap1);
	// ���s�D��x�ݒ�
	iwin_set_priority(_iPriority);
	// SQL�쐬
	// SJIS �ō쐬�iDOS�v�����v�g�Ή��j
	i1 = imi_len(SELECT_VIEW) + imi_len(_sSelect) + imi_len(_sWhere1) + imi_len(_sWhere2) + imi_len(_sGroup) + imi_len(_sSort) + 1;
	snprintf($sTmp, IGET_ARGS_LEN_MAX, SELECT_VIEW, _sSelect, _sWhere1, _sWhere2, _sGroup, _sSort);
	// SJIS �� UTF-8 �ɕϊ��iSqlite3�Ή��j
	$sqlU = M2U($sTmp);
	// -table "header"
	if(*_sTable1)
	{
		P2(_sTable1); // ""�ȊO�̂Ƃ�
	}
	// -in DB���w��
	// UTF-8
	U8N *up1 = M2U(_sInDbn);
		if(sqlite3_open(up1, &$iDbs))
		{
			fprintf(stderr, "[Err] -in '%s' ���J���Ȃ�!\n", _sInDbn);
			sqlite3_close($iDbs); // Err�ł�DB���
			imain_end();
		}
		else
		{
			// DB�\�z
			if(!*_sIn)
			{
				$struct_iFinfoW *FI = iFinfo_allocW();
					/*
					�y�d�v�zUTF-8��DB�\�z
						�f�[�^����(INSERT)�́A
							�EPRAGMA encoding = 'TTF-16le'
							�Ebind_text16()
						���g�p���āA"UTF-16LE"�œo�^�\�����A
						�o��(SELECT)�́Aexec()��"UTF-8"��������Ȃ����ߖ{���_�ł́A
							�����́� UTF-16 => UTF-8
							��SQL��  SJIS   => UTF-8
							���o�́� UTF-8  => SJIS
						�̕��@�ɒH�蒅�����B
					*/
					sql_exec($iDbs, "PRAGMA encoding='UTF-8';", NULL);
					// TABLE�쐬
					sql_exec($iDbs, CREATE_T_DIR, NULL);
					sql_exec($iDbs, CREATE_T_FILE, NULL);
					// VIEW�쐬
					sql_exec($iDbs, CREATE_VIEW, NULL);
					// �O����
					sqlite3_prepare($iDbs, INSERT_T_DIR, imi_len(INSERT_T_DIR), &$stmt1, NULL);
					sqlite3_prepare($iDbs, INSERT_T_FILE, imi_len(INSERT_T_FILE), &$stmt2, NULL);
					// �g�����U�N�V�����J�n
					sql_exec($iDbs, "BEGIN", NULL);
					// �����f�[�^ DB����
					WCS *wp1 = 0;
					for(i2 = 0; (p1 = *(_aDir + i2)); i2++)
					{
						wp1 = M2W(p1);
							$uStepCnt = iwi_len(wp1);
							ifind10(FI, wp1, $uStepCnt, 0); // �{����
						ifree(wp1);
					}
					// �g�����U�N�V�����I��
					sql_exec($iDbs, "COMMIT", NULL);
					// �㏈��
					sqlite3_finalize($stmt2);
					sqlite3_finalize($stmt1);
				iFinfo_freeW(FI);
			}
			// ����
			if(*_sOut)
			{
				// ���݂���ꍇ�A�폜
				irm_file(_sOutDbn);
				// _sIn, _sOut���w��A�ʃt�@�C�����̂Ƃ�
				if(*_sIn)
				{
					CopyFile(_sInDbn, OLDDB, FALSE);
				}
				// WHERE STR �̂Ƃ��s�v�f�[�^�폜
				if(imi_len(_sWhere0))
				{
					sql_exec($iDbs, "BEGIN", NULL); // �g�����U�N�V�����J�n
						p1 = ims_cat_clone("WHERE ", _sWhere0);
							snprintf($sTmp, IGET_ARGS_LEN_MAX, UPDATE_EXEC99_1, p1);
						ifree(p1);
						p1 = M2U($sTmp); // UTF-8�ŏ���
							sql_exec($iDbs, p1, NULL);              // �t���O�𗧂Ă�
							sql_exec($iDbs, DELETE_EXEC99_1, NULL); // �s�v�f�[�^�폜
							sql_exec($iDbs, UPDATE_EXEC99_2, NULL); // �t���O������
						ifree(p1);
					sql_exec($iDbs, "COMMIT", NULL); // �g�����U�N�V�����I��
					sql_exec($iDbs, "VACUUM", NULL); // VACUUM
				}
				// _sIn, _sOut ���w��̂Ƃ���, �r��, �t�@�C�������t�ɂȂ�̂�, ���swap
				sql_saveOrLoadMemdb($iDbs, (*_sIn ? _sInDbn : _sOutDbn), TRUE);
				// outDb �o�͐��J�E���g
				_iExec = 99;
				sql_exec($iDbs, "SELECT number FROM V_INDEX;", print_result); // "SELECT *" �͒x��
			}
			else
			{
				// ��ʕ\��
				sql_exec($iDbs, $sqlU, print_result);
			}
			// DB���
			sqlite3_close($iDbs);
			// _sIn, _sOut �t�@�C������swap
			if(*_sIn)
			{
				MoveFile(_sInDbn, _sOutDbn);
				MoveFile(OLDDB, _sInDbn);
			}
			// ��Ɨp�t�@�C���폜
			DeleteFile(OLDDB);
		}
	ifree(up1);
	//
	// -table "footer"
	//
	if(*_sTable2)
	{
		P2(_sTable2); // ""�ȊO�̂Ƃ�
	}
	//
	// �t�b�^��
	//
	if(_bNoGuide || _bHtml)
	{
		// footer��\�����Ȃ�
	}
	else
	{
		print_footer();
	}
	//
	// �f�o�b�O�p
	//
	///icalloc_mapPrint();ifree_all();icalloc_mapPrint();
	//
	imain_end();
}

MBS
*str_encode(
	MBS *pM
)
{
	MBS *p1 = ims_clone(pM);
	MBS *p2 = 0;
	p2 = ijs_replace(p1, "\\a", "\a"); // "\a"
		ifree(p1);
		p1 = p2;
	p2 = ijs_replace(p1, "\\t", "\t"); // "\t"
		ifree(p1);
		p1 = p2;
	p2 = ijs_replace(p1, "\\n", "\n"); // "\n"
		ifree(p1);
		p1 = p2;
	return p1;
}

MBS
*sql_escape(
	MBS *pM
)
{
	MBS *p1 = ims_clone(pM);
	MBS *p2 = 0;
		p2 = ijs_replace(p1, ";", " "); // ";" => " " SQL�C���W�F�N�V�������
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
			_aiNow[0],
			_aiNow[1],
			_aiNow[2],
			_aiNow[3],
			_aiNow[4],
			_aiNow[5]
		); // [] ����t�ɕϊ�
	ifree(p1);
		p1 = p2;
	return p1;
}

/* 2016-08-19
�y���ӁzDir�̕\���ɂ���
	d:\aaa\ �ȉ��́A
		d:\aaa\bbb\
		d:\aaa\ccc\
		d:\aaa\ddd.txt
	��\�������Ƃ��̈Ⴂ�͎��̂Ƃ���B
	�@iwmls.exe�ils�Adir�݊��j
		d:\aaa\bbb\
		d:\aaa\ccc\
		d:\aaa\ddd.txt
	�Aiwmfind.exe�iBaseDir��File��\���j
		d:\aaa\
		d:\aaa\ddd.txt
	��Dir��File��ʃe�[�u���ŊǗ��^join���Ďg�p���邽�߁A
	�@���̂悤�Ȏd�l�ɂȂ炴��𓾂Ȃ��B
*/
VOID
ifind10(
	$struct_iFinfoW *FI,
	WCS *dir,
	UINT dirLenJ,
	INT depth
)
{
	if(depth > _iMaxDepth)
	{
		return;
	}
	WIN32_FIND_DATAW F;
	WCS *p1 = iwp_cpy(FI->fullnameW, dir);
		iwp_cpy(p1, L"*");
	HANDLE hfind = FindFirstFileW(FI->fullnameW, &F);
		// �ǂݔ�΂� Depth
		BOOL bMinDepthFlg = (depth >= _iMinDepth ? TRUE : FALSE);
		// dirId�𒀎�����
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
					p1 = iws_clone(FI->fullnameW);
						// ����Dir��
						ifind10(FI, p1, FI->iEnd, depth + 1);
					ifree(p1);
				}
				else if(bMinDepthFlg)
				{
					ifind22(FI, dirId, F.cFileName);
				}
			}
		}
		while(FindNextFileW(hfind, &F));
	FindClose(hfind);
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

/*
	SQL�̎��s
*/
VOID
sql_exec(
	sqlite3 *db,
	const MBS *sql,
	sqlite3_callback cb
)
{
	MBS *p_err = 0; // SQLite���g�p
	$uRowCnt = 0;
	if(sqlite3_exec(db, sql, cb, NULL, &p_err))
	{
		fprintf(stderr, "[Err] �\\���G���[\n    %s\n    %s\n", p_err, sql);
		sqlite3_free(p_err); // p_err�����
		imain_end();
	}
}

/*
	�������ʂ��P�s�擾����x�ɌĂ΂��R�[���o�b�N�֐�
*/
INT
print_result(
	VOID *option,
	INT iColumnCount,
	MBS **sColumnValues,
	MBS **sColumnNames
)
{
	INT i1 = 0;
	MBS *p1 = 0, *p1e = 0, *p2 = 0, *p3 = 0, *p4 = 0, *p5 = 0;
	INT iColumnCount2 = iColumnCount-1;
	switch(_iExec)
	{
		case(I_MKDIR): // --mkdir
		case(I_CP):    // --copy
		case(I_MV):    // --move
		case(I_MV2):   // --move2
			// _sOpMd\�ȉ��ɂ́A_aDir\�ȉ��� dir ���쐬
			i1 = atoi(*(sColumnValues + 0)); // step_cnt
			p1 = U2M(*(sColumnValues + 1));  // dir "\"�t
			p1e = ijp_forwardN(p1, i1);      // p1��EOD
			p2 = U2M(*(sColumnValues + 2));  // name
			p3 = U2M(*(sColumnValues + 3));  // path
			// mkdir
			snprintf($sTmp, IGET_ARGS_LEN_MAX, "%s\\%s", _sOpMd, p1e);
				if(imk_dir($sTmp))
				{
					P("md   => %s\n", $sTmp); // �V�Kdir��\��
					++$uRowCnt;
				}
			// ��
			p4 = ims_clone(p1e);
				snprintf($sTmp, IGET_ARGS_LEN_MAX, "%s\\%s%s", _sOpMd, p4, p2);
			p5 = ims_clone($sTmp);
			if(_iExec == I_CP)
			{
				if(CopyFile(p3, p5, FALSE))
				{
					P("cp   <= %s\n", p3); // file��\��
					++$uRowCnt;
				}
			}
			else if(_iExec >= I_MV)
			{
				// ReadOnly����(1)������
				if((1 & GetFileAttributes(p5)))
				{
					SetFileAttributes(p5, FALSE);
				}
				// ����file�폜
				if(iFchk_typePathA(p5))
				{
					irm_file(p5);
				}
				if(MoveFile(p3, p5))
				{
					P("mv   <= %s\n", p3); // file��\��
					++$uRowCnt;
				}
				// rmdir
				if(_iExec == I_MV2)
				{
					if(irm_dir(p1))
					{
						P("rd   => %s\n", p1); // dir��\��
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
			p1 = U2M(*(sColumnValues + 0)); // path
			p2 = U2M(*(sColumnValues + 1)); // name
				// mkdir
				if(imk_dir(_sOpMd))
				{
					P("md   => %s\n", _sOpMd); // �V�Kdir��\��
					++$uRowCnt;
				}
				// ��
				snprintf($sTmp, IGET_ARGS_LEN_MAX, "%s\\%s", _sOpMd, p2);
				p3 = ims_clone($sTmp);
					// I_EXT1, I_EXT2���A����file�͏㏑��
					if(_iExec == I_EXT1)
					{
						if(CopyFile(p1, p3, FALSE))
						{
							P("ext1 <= %s\n", p3); // file��\��
							++$uRowCnt;
						}
					}
					else if(_iExec == I_EXT2)
					{
						// ReadOnly����(1)������
						if((1 & GetFileAttributes(p3)))
						{
							SetFileAttributes(p3, FALSE);
						}
						// dir���݂��Ă���΍폜���Ă���
						if(iFchk_typePathA(p3))
						{
							irm_file(p3);
						}
						if(MoveFile(p1, p3))
						{
							P("mv   <= %s\n", p3); // file��\��
							++$uRowCnt;
						}
					}
				ifree(p3);
			ifree(p2);
			ifree(p1);
		break;
		case(I_RP): // --replace
			p1 = U2M(*(sColumnValues + 0)); // type
			p2 = U2M(*(sColumnValues + 1)); // path
				if(*p1 == 'f' && CopyFile(_sOpRep, p2, FALSE))
				{
					P("rep  => %s\n", p2); // file��\��
					++$uRowCnt;
				}
			ifree(p2);
			ifree(p1);
		break;
		case(I_RM):  // --remove
		case(I_RM2): // --remove2
			p1 = U2M(*(sColumnValues + 0)); // path
			p2 = U2M(*(sColumnValues + 1)); // dir
			// ReadOnly����(1)������
			if((1 & atoi(*(sColumnValues + 2))))
			{
				SetFileAttributes(p1, FALSE);
			}
			// file �폜
			if(irm_file(p1))
			{
				// file�̂�
				P("rm   => %s\n", p1); // file��\��
				++$uRowCnt;
			}
			// ��dir �폜
			if(_iExec == I_RM2)
			{
				// ��dir�ł���
				if(irm_dir(p2))
				{
					P("rd   => %s\n", p2); // dir��\��
					++$uRowCnt;
				}
			}
			ifree(p2);
			ifree(p1);
		break;
		case(99): // Count Only
			++$uRowCnt;
		break;
		default:
			// �^�C�g���\��
			if(!$uRowCnt)
			{
				iConsole_setTextColor(ColorExp3);
					P20(_sTr1); // �s�擪�̕t��������
					for(i1 = 0; i1 < iColumnCount; i1++)
					{
						p1 = *(sColumnNames + i1);
						p2 = U2M(p1);
							P(
								"%s%s[%s]%s%s",
								_sTd1,
								_sQuote,
								p2,
								_sQuote,
								(!*_sTd1 && i1 == iColumnCount2 ? "" : _sTd2)
							);
						ifree(p2);
					}
					P20(_sTr2); // �s�����̕t��������
				iConsole_setTextColor(_getColorDefault);
				NL();
			}
			// �f�[�^�\��
			P20(_sTr1); // �s�擪�̕t��������
			for(i1 = 0; i1 < iColumnCount; i1++)
			{
				// [number]
				if(i1 == _iSelectPosNum)
				{
					P(
						"%s%s%u%s%s",
						_sTd1,
						_sQuote,
						$uRowCnt + 1,
						_sQuote,
						(!*_sTd1 && i1 == iColumnCount2 ? "" : _sTd2)
					);
				}
				else
				{
					p2 = U2M(*(sColumnValues + i1));
						P(
							"%s%s%s%s%s",
							_sTd1,
							_sQuote,
							p2,
							_sQuote,
							(!*_sTd1 && i1 == iColumnCount2 ? "" : _sTd2)
						);
					ifree(p2);
				}
			}
			P2(_sTr2); // �s�����̕t��������
			++$uRowCnt;
		break;
	}
	return SQLITE_OK; // return 0
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
	// UTF-8�ŏ���
	U8N *up1 = M2U(ofn);
		if(!(err = sqlite3_open(up1, &pFile)))
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
	return (!err ? TRUE : FALSE);
}

VOID
print_footer()
{
	MBS *p1 = 0;
	iConsole_setTextColor(ColorExp3);
		LN();
		P(
			"-- %u row%s in set ( %.3f sec)\n",
			$uRowCnt,
			($uRowCnt > 1 ? "s" : ""), // �����`
			iExecSec_next($execMS)
		);
	iConsole_setTextColor(ColorExp1);
		P2("--");
			UINT u1 = 0;
			while(u1 < _aDirNum)
			{
				P("--  _aDir(%u) \"%s\"\n", u1 + 1, *(_aDir + u1));
				++u1;
			}
		P2("--");
			p1 = U2M($sqlU);
				P("--  $sqlU  \"%s\"\n", p1);
			ifree(p1);
		P2("--");
		if(*_sInDbn)
		{
			P("--  -in    \"%s\"\n", _sInDbn);
		}
		if(*_sOutDbn)
		{
			P("--  -out   \"%s\"\n", _sOutDbn);
		}
		if(_bNoGuide)
		{
			P("--  -noguide\n");
		}
		if(*_sQuote)
		{
			P("--  -quote \"%s\"\n", _sQuote);
		}
		if(*_sTable1 || *_sTable2)
		{
			P("--  -table \"%s\" \"%s\"\n", _sTable1, _sTable2);
		}
		if(*_sTr1 || *_sTr2)
		{
			P("--  -tr    \"%s\" \"%s\"\n", _sTr1, _sTr2);
		}
		if(*_sTd1 || *_sTd2)
		{
			P("--  -td    \"%s\" \"%s\"\n", _sTd1, _sTd2);
		}
		if(_bHtml)
		{
			P("--  -html\n");
		}
			P("--  ---priority \"%d\"\n", _iPriority);
		P2("--");
	iConsole_setTextColor(_getColorDefault);
}

VOID
print_version()
{
	LN();
	P("  %s\n",
		IWMFIND_COPYRIGHT
	);
	P("    Ver.%s+%s+SQLite%s\n",
		IWMFIND_VERSION,
		LIB_IWMUTIL_VERSION,
		SQLITE_VERSION
	);
}

VOID
print_help()
{
	iConsole_setTextColor(ColorHeaderFooter);
		print_version();
		LN();
	iConsole_setTextColor(ColorBgText1);
		P (" %s [Dir] [Option] ", $program);
	iConsole_setTextColor(ColorText1);
		P9(2);
	iConsole_setTextColor(ColorText2);
		P2(" (�g�p��1) ���� ");
		P ("   %s DIR -r -s \"number, path, size\" -w \"ext like 'exe'\" ", $program);
		P9(2);
		P2(" (�g�p��2) �������ʂ��t�@�C���֕ۑ� ");
		P ("   %s DIR1 DIR2 ... -r -o FILE [Option] ", $program);
		P9(2);
		P2(" (�g�p��3) �����Ώۂ��t�@�C������Ǎ� ");
		P ("   %s -i FILE [Option] ", $program);
	iConsole_setTextColor(ColorText1);
		P9(2);
	iConsole_setTextColor(ColorExp2);
		P2("[Dir]");
	iConsole_setTextColor(ColorText1);
		P2("    �����Ώ� dir");
		P2("    (��) �����w��̏ꍇ�A���Dir�ɏW�񂷂�");
		P ("    (��) \"c:\\\" \"c:\\windows\\\" => \"c:\\\"\n");
		NL();
	iConsole_setTextColor(ColorExp2);
		P2("[Option]");
	iConsole_setTextColor(ColorExp3);
		P2("-out | -o FILE");
	iConsole_setTextColor(ColorText1);
		P2("    �o�̓t�@�C��");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-in | -i FILE");
	iConsole_setTextColor(ColorText1);
		P2("    ���̓t�@�C��");
		P2("    (��) �����Ώ� dir �ƕ��p�ł��Ȃ�");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-select | -s \"COLUMN1, ...\"");
	iConsole_setTextColor(ColorText1);
		P2("    number    (NUM) �\\����");
		P2("    path      (STR) dir\\name");
		P2("    dir       (STR) �f�B���N�g����");
		P2("    name      (STR) �t�@�C����");
		P2("    ext       (STR) �g���q");
		P2("    depth     (NUM) �f�B���N�g���K�w = 0..");
		P2("    type      (STR) �f�B���N�g�� = d�^�t�@�C�� = f");
		P2("    attr_num  (NUM) ����");
		P2("    attr      (STR) ���� \"[d|f][r|-][h|-][s|-][a|-]\"");
		P2("                      [dir|file][read-only][hidden][system][archive]");
		P2("    size      (NUM) �t�@�C���T�C�Y = byte");
		P2("    ctime_cjd (NUM) �쐬����     -4712/01/01 00:00:00�n�_�̒ʎZ���^CJD=JD-0.5");
		P2("    ctime     (STR) �쐬����     \"yyyy-mm-dd hh:nn:ss\"");
		P2("    mtime_cjd (NUM) �X�V����     ctime_cjd�Q��");
		P2("    mtime     (STR) �X�V����     ctime�Q��");
		P2("    atime_cjd (NUM) �A�N�Z�X���� ctime_cjd�Q��");
		P2("    atime     (STR) �A�N�Z�X���� ctime�Q��");
		NL();
		P2("    ��1 COLUMN�w��Ȃ��̏ꍇ");
		P ("       %s ���ŕ\\��\n", OP_SELECT_1);
		NL();
		P2("    ��2 SQLite���Z�q�^�֐��𗘗p�\\");
		P2("       (��)");
		P2("            abs(X)  changes()  char(X1, X2, ..., XN)  coalesce(X, Y, ...)");
		P2("            glob(X, Y)  ifnull(X, Y)  instr(X, Y)  hex(X)");
		P2("            last_insert_rowid()  length(X)");
		P2("            like(X, Y)  like(X, Y, Z)  likelihood(X, Y)  likely(X)");
		P2("            load_extension(X)  load_extension(X, Y)  lower(X)");
		P2("            ltrim(X)  ltrim(X, Y)  max(X, Y, ...)  min(X, Y, ...)");
		P2("            nullif(X, Y)  printf(FORMAT, ...)  quote(X)");
		P2("            random()  randomblob(N)  replace(X, Y, Z)");
		P2("            round(X)  round(X, Y)  rtrim(X)  rtrim(X, Y)");
		P2("            soundex(X)  sqlite_compileoption_get(N)");
		P2("            sqlite_compileoption_used(X)  sqlite_source_id()");
		P2("            sqlite_version()  substr(X, Y, Z) substr(X, Y)");
		P2("            total_changes()  trim(X) trim(X, Y)  typeof(X)  unlikely(X)");
		P2("            unicode(X)  upper(X)  zeroblob(N)");
		P2("       (��) �}���`�o�C�g�������P�����Ƃ��ď���.");
		P2("       (�Q�l) http://www.sqlite.org/lang_corefunc.html");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-where | -w STR");
	iConsole_setTextColor(ColorText1);
		P2("    (��1) \"size<=100 or size>1000000\"");
		P2("    (��2) \"type like 'f' and name like 'abc??.*'\"");
		P2("          '?' '_' �͔C�ӂ�1����");
		P2("          '*' '%' �͔C�ӂ�0�����ȏ�");
		P2("    (��3) ��� \"2010-12-10 12:00:00\"�̂Ƃ�");
		P2("          \"ctime>[-10D]\"   => \"ctime>'2010-11-30 00:00:00'\"");
		P2("          \"ctime>[-10d]\"   => \"ctime>'2010-11-30 12:00:00'\"");
		P2("          \"ctime>[-10d*]\"  => \"ctime>'2010-11-30 %%'\"");
		P2("          \"ctime>[-10d%%]\" => \"ctime>'2010-11-30 %%'\"");
		P2("          (�N) Y, y (��) M, m (��) D, d (��) H, h (��) N, n (�b) S, s");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-group | -g STR");
	iConsole_setTextColor(ColorText1);
		P2("    (��) -g \"STR1, STR2\"");
		P2("         STR1��STR2���O���[�v���ɂ܂Ƃ߂�");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-sort | -st STR [ASC|DESC]");
	iConsole_setTextColor(ColorText1);
		P2("    (��) -st \"STR1 ASC, STR2 DESC\"");
		P2("         STR1�����\\�[�g, STR2���t���\\�[�g");
		NL();
	iConsole_setTextColor(ColorExp3);
		P2("-recursive | -r *NUM");
	iConsole_setTextColor(ColorText1);
		P2("    ���K�w��S����");
		P2("    �I�v�V���� *NUM �w��̂Ƃ��́A*NUM �K�w�܂Ō���");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-depth | -d NUM1 NUM2");
	iConsole_setTextColor(ColorText1);
		P2("    �������鉺�K�w���w��");
		P2("    (��1) -d \"1\"");
		P2("          1�K�w�̂݌���");
		P2("    (��2) -d \"3\" \"5\"");
		P2("          3�`5�K�w������");
		NL();
		P2("    ��1 CurrentDir �� \"0\"");
		P2("    ��2 �u-depth�v�Ɓu-where���ł�depth�v�̋����̈Ⴂ");
		P2("      ��(����) -depth �͎w�肳�ꂽ�K�w�̂݌������s��");
		P2("      ��(�x��) -where���ł�depth�ɂ�錟���́A�S�K�w��dir�^file�ɑ΂��čs��");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-noguide | -ng");
	iConsole_setTextColor(ColorText1);
		P2("    �t�b�^����\\�����Ȃ�");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-quote | -qt STR");
	iConsole_setTextColor(ColorText1);
		P2("    �͂ݕ���");
		P2("    (��) -qt \"'\"");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-table STR1 STR2");
	iConsole_setTextColor(ColorText1);
		P2("    �w�b�_(STR1)�A�t�b�^(STR2)��t��");
		P2("    (��1) -table \"<table>\" \"</table>\"");
		P2("    (��2) -table \"\" \"footer\"");
		P2("      �����̉��s��\"\\n\"���g�p");
		P2("      �� �擪��\"-\"�́A\"\\-\"�ƕ\\�L");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-tr STR1 STR2");
	iConsole_setTextColor(ColorText1);
		P2("    �s�O��ɕ�����t��");
		P2("    (��1) -tr \"<tr>\" \"</tr>\"");
		P2("    (��2) -tr \",\"");
		P2("    �� �擪��\"-\"�́A\"\\-\"�ƕ\\�L");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-td STR1 STR2");
	iConsole_setTextColor(ColorText1);
		P2("    ��؂蕶���^STR2�̓I�v�V����");
		P2("    (��1) -td \"<td>\" \"</td>\"");
		P2("    (��2) -td \",\"");
		P2("    �� �擪��\"-\"�́A\"\\-\"�ƕ\\�L");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("-html");
	iConsole_setTextColor(ColorText1);
		P2("    HTML Table�o��");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--mkdir | --md DIR");
	iConsole_setTextColor(ColorText1);
		P2("    �������ʂ���dir�𒊏o��DIR�ɍ쐬����(�K�w�ێ�)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--copy | --cp DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + �������ʂ�DIR�ɃR�s�[����(�K�w�ێ�)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--move | --mv DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + �������ʂ�DIR�Ɉړ�����(�K�w�ێ�)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--move2 | --mv2 DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + --move + �ړ����̋�dir���폜����(�K�w�ێ�)");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--extract1 | --ext1 DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + �������ʃt�@�C���̂ݒ��o��DIR�ɃR�s�[����");
		P2("    (��) �K�w���ێ����Ȃ��^�����t�@�C���͏㏑��");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--extract2 | --ext2 DIR");
	iConsole_setTextColor(ColorText1);
		P2("    --mkdir + �������ʃt�@�C���̂ݒ��o��DIR�Ɉړ�����");
		P2("    (��) �K�w���ێ����Ȃ��^�����t�@�C���͏㏑��");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--remove | --rm");
	iConsole_setTextColor(ColorText1);
		P2("    �������ʂ� file �̂ݍ폜����");
		P2("    �� dir�͍폜���Ȃ�");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--remove2 | --rm2");
	iConsole_setTextColor(ColorText1);
		P2("    --remove + ��dir(subDir����) ���폜����");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("--replace | --rep FILE");
	iConsole_setTextColor(ColorText1);
		P2("    ��������(����) �� FILE(�P�ꖼ) �̓��e�Œu���^�t�@�C�����͕ύX���Ȃ�");
		P2("    (��) -w \"name like 'foo.txt'\" --rep \".\\foo.txt\"");
		P2("    �� �������ʂ͏㏑�������̂Œ���");
	iConsole_setTextColor(ColorExp3);
		NL();
		P2("---priority | ---pr NUM");
	iConsole_setTextColor(ColorText1);
		P2("    ���s�D��x��ݒ�");
		P2("    (��)NUM 1=�D��^2=�ʏ�ȏ�^3=�ʏ�(�����l)�^4=�ʏ�ȉ��^5=�A�C�h����");
	iConsole_setTextColor(ColorHeaderFooter);
		NL();
		LN();
	iConsole_setTextColor(_getColorDefault);
}
