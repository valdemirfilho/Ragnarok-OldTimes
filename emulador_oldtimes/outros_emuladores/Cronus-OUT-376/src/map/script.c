// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

//#define DEBUG_FUNCIN
//#define DEBUG_DISP
//#define DEBUG_RUN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <time.h>

#include "../common/socket.h"
#include "../common/timer.h"
#include "../common/malloc.h"
#include "../common/lock.h"
#include "../common/nullpo.h"
#include "../common/showmsg.h"
#include "../common/strlib.h"

#include "map.h"
#include "clif.h"
#include "chrif.h"
#include "itemdb.h"
#include "pc.h"
#include "status.h"
#include "script.h"
#include "storage.h"
#include "mob.h"
#include "npc.h"
#include "pet.h"
#include "intif.h"
#include "skill.h"
#include "chat.h"
#include "battle.h"
#include "party.h"
#include "guild.h"
#include "atcommand.h"
#include "charcommand.h"
#include "log.h"
#include "unit.h"

#define SCRIPT_BLOCK_SIZE 256

#define FETCH(n, t) \
		if(st->end>st->start+(n)) \
			(t)=conv_num(st,&(st->stack->stack_data[st->start+(n)]));

enum { LABEL_NEXTLINE=1,LABEL_START };
static unsigned char * script_buf = NULL;
static int script_pos,script_size;

char *str_buf;
int str_pos,str_size;
static struct str_data_struct {
	int type;
	int str;
	int backpatch;
	int label;
	int (*func)(struct script_state *);
	int val;
	int next;
} *str_data = NULL;
int str_num=LABEL_START,str_data_size;
int str_hash[16];

static struct dbt *mapreg_db=NULL;
static struct dbt *mapregstr_db=NULL;
static int mapreg_dirty=-1;
char mapreg_txt[256]="save/mapreg.txt";
#define MAPREG_AUTOSAVE_INTERVAL	(10*1000)

static struct dbt *scriptlabel_db=NULL;
static struct dbt *userfunc_db=NULL;

struct dbt* script_get_label_db(){ return scriptlabel_db; }
struct dbt* script_get_userfunc_db(){ return userfunc_db; }

static char pos[11][100] = {"頭","体","左手","右手","ローブ","靴","アクセサリー1","アクセサリー2","頭2","頭3","装着していない"};

struct Script_Config script_config;

static int parse_cmd;

// for advanced scripting support ( nested if, switch, while, for, do-while, function, etc )
// [Eoe / jA 1080, 1081, 1094, 1164]
enum { TYPE_NULL = 0 , TYPE_IF , TYPE_SWITCH , TYPE_WHILE , TYPE_FOR , TYPE_DO , TYPE_USERFUNC};
static struct {
	struct {
		int type;
		int index;
		int count;
		int flag;
	} curly[256];		// 右カッコの情報
	int curly_count;	// 右カッコの数
	int index;			// スクリプト内で使用した構文の数
} syntax;
unsigned char* parse_curly_close(unsigned char *p);
unsigned char* parse_syntax_close(unsigned char *p);
unsigned char* parse_syntax_close_sub(unsigned char *p,int *flag);
unsigned char* parse_syntax(unsigned char *p);
static int parse_syntax_for_flag = 0;

extern int current_equip_item_index; //for New CARS Scripts. It contains Inventory Index of the EQUIP_SCRIPT caller item. [Lupus]
int potion_flag=0; //For use on Alchemist improved potions/Potion Pitcher. [Skotlex]
int potion_hp=0, potion_per_hp=0, potion_sp=0, potion_per_sp=0;
int potion_target=0;

#if !defined(TXT_ONLY) && defined(MAPREGSQL)
// [zBuffer] SQL Mapreg Saving/Loading Database Declaration
char mapregsql_db[32] = "mapreg";
char mapregsql_db_varname[32] = "varname";
char mapregsql_db_index[32] = "index";
char mapregsql_db_value[32] = "value";
char tmp_sql[65535];
// --------------------------------------------------------
#endif

/*==========================================
 * ローカルプロトタイプ宣言 (必要な物のみ)
 *------------------------------------------
 */
unsigned char* parse_subexpr(unsigned char *,int);
#ifndef TXT_ONLY
int buildin_query_sql(struct script_state *st);
int buildin_escape_sql(struct script_state *st);
#endif
int buildin_atoi(struct script_state *st);
int buildin_axtoi(struct script_state *st);
int buildin_mes(struct script_state *st);
int buildin_goto(struct script_state *st);
int buildin_callsub(struct script_state *st);
int buildin_callfunc(struct script_state *st);
int buildin_return(struct script_state *st);
int buildin_getarg(struct script_state *st);
int buildin_next(struct script_state *st);
int buildin_close(struct script_state *st);
int buildin_close2(struct script_state *st);
int buildin_menu(struct script_state *st);
int buildin_menu2(struct script_state *st);
int buildin_rand(struct script_state *st);
int buildin_warp(struct script_state *st);
int buildin_areawarp(struct script_state *st);
int buildin_warpchar(struct script_state *st); // [LuzZza]
int buildin_warpparty(struct script_state *st); //[Fredzilla]
int buildin_warpguild(struct script_state *st); //[Fredzilla]
int buildin_heal(struct script_state *st);
int buildin_itemheal(struct script_state *st);
int buildin_percentheal(struct script_state *st);
int buildin_jobchange(struct script_state *st);
int buildin_input(struct script_state *st);
int buildin_setlook(struct script_state *st);
int buildin_set(struct script_state *st);
int buildin_setarray(struct script_state *st);
int buildin_cleararray(struct script_state *st);
int buildin_copyarray(struct script_state *st);
int buildin_getarraysize(struct script_state *st);
int buildin_deletearray(struct script_state *st);
int buildin_getelementofarray(struct script_state *st);
int buildin_getitem(struct script_state *st);
int buildin_getitem2(struct script_state *st);
int buildin_getnameditem(struct script_state *st);
int buildin_grouprandomitem(struct script_state *st);
int buildin_makeitem(struct script_state *st);
int buildin_delitem(struct script_state *st);
int buildin_delitem2(struct script_state *st);
int buildin_enableitemuse(struct script_state *st);
int buildin_disableitemuse(struct script_state *st);
int buildin_viewpoint(struct script_state *st);
int buildin_countitem(struct script_state *st);
int buildin_countitem2(struct script_state *st);
int buildin_checkweight(struct script_state *st);
int buildin_readparam(struct script_state *st);
int buildin_getcharid(struct script_state *st);
int buildin_getpartyname(struct script_state *st);
int buildin_getpartymember(struct script_state *st);
int buildin_getguildname(struct script_state *st);
int buildin_getguildmaster(struct script_state *st);
int buildin_getguildmasterid(struct script_state *st);
int buildin_strcharinfo(struct script_state *st);
int buildin_getequipid(struct script_state *st);
int buildin_getequipname(struct script_state *st);
int buildin_getbrokenid(struct script_state *st); // [Valaris]
int buildin_repair(struct script_state *st); // [Valaris]
int buildin_getequipisequiped(struct script_state *st);
int buildin_getequipisenableref(struct script_state *st);
int buildin_getequipisidentify(struct script_state *st);
int buildin_getequiprefinerycnt(struct script_state *st);
int buildin_getequipweaponlv(struct script_state *st);
int buildin_getequippercentrefinery(struct script_state *st);
int buildin_successrefitem(struct script_state *st);
int buildin_failedrefitem(struct script_state *st);
int buildin_cutin(struct script_state *st);
int buildin_cutincard(struct script_state *st);
int buildin_statusup(struct script_state *st);
int buildin_statusup2(struct script_state *st);
int buildin_bonus(struct script_state *st);
int buildin_bonus2(struct script_state *st);
int buildin_bonus3(struct script_state *st);
int buildin_bonus4(struct script_state *st);
int buildin_skill(struct script_state *st);
int buildin_addtoskill(struct script_state *st); // [Valaris]
int buildin_guildskill(struct script_state *st);
int buildin_getskilllv(struct script_state *st);
int buildin_getgdskilllv(struct script_state *st);
int buildin_basicskillcheck(struct script_state *st);
int buildin_getgmlevel(struct script_state *st);
int buildin_end(struct script_state *st);
int buildin_checkoption(struct script_state *st);
int buildin_setoption(struct script_state *st);
int buildin_setcart(struct script_state *st);
int buildin_checkcart(struct script_state *st); // check cart [Valaris]
int buildin_setfalcon(struct script_state *st);
int buildin_checkfalcon(struct script_state *st); // check falcon [Valaris]
int buildin_setriding(struct script_state *st);
int buildin_checkriding(struct script_state *st); // check for pecopeco [Valaris]
int buildin_savepoint(struct script_state *st);
int buildin_gettimetick(struct script_state *st);
int buildin_gettime(struct script_state *st);
int buildin_gettimestr(struct script_state *st);
int buildin_openstorage(struct script_state *st);
int buildin_guildopenstorage(struct script_state *st);
int buildin_itemskill(struct script_state *st);
int buildin_produce(struct script_state *st);
int buildin_monster(struct script_state *st);
int buildin_areamonster(struct script_state *st);
int buildin_killmonster(struct script_state *st);
int buildin_killmonsterall(struct script_state *st);
int buildin_clone(struct script_state *st);
int buildin_doevent(struct script_state *st);
int buildin_donpcevent(struct script_state *st);
int buildin_addtimer(struct script_state *st);
int buildin_deltimer(struct script_state *st);
int buildin_addtimercount(struct script_state *st);
int buildin_initnpctimer(struct script_state *st);
int buildin_stopnpctimer(struct script_state *st);
int buildin_startnpctimer(struct script_state *st);
int buildin_setnpctimer(struct script_state *st);
int buildin_getnpctimer(struct script_state *st);
int buildin_attachnpctimer(struct script_state *st);	// [celest]
int buildin_detachnpctimer(struct script_state *st);	// [celest]
int buildin_playerattached(struct script_state *st);	// [Skotlex]
int buildin_announce(struct script_state *st);
int buildin_mapannounce(struct script_state *st);
int buildin_areaannounce(struct script_state *st);
int buildin_getusers(struct script_state *st);
int buildin_getmapusers(struct script_state *st);
int buildin_getareausers(struct script_state *st);
int buildin_getareadropitem(struct script_state *st);
int buildin_enablenpc(struct script_state *st);
int buildin_disablenpc(struct script_state *st);
int buildin_enablearena(struct script_state *st);	// Added by RoVeRT
int buildin_disablearena(struct script_state *st);	// Added by RoVeRT
int buildin_hideoffnpc(struct script_state *st);
int buildin_hideonnpc(struct script_state *st);
int buildin_sc_start(struct script_state *st);
int buildin_sc_start2(struct script_state *st);
int buildin_sc_start4(struct script_state *st);
int buildin_sc_end(struct script_state *st);
int buildin_getscrate(struct script_state *st);
int buildin_debugmes(struct script_state *st);
int buildin_catchpet(struct script_state *st);
int buildin_birthpet(struct script_state *st);
int buildin_resetlvl(struct script_state *st);
int buildin_resetstatus(struct script_state *st);
int buildin_resetskill(struct script_state *st);
int buildin_skillpointcount(struct script_state *st);
int buildin_changebase(struct script_state *st);
int buildin_changesex(struct script_state *st);
int buildin_waitingroom(struct script_state *st);
int buildin_delwaitingroom(struct script_state *st);
int buildin_enablewaitingroomevent(struct script_state *st);
int buildin_disablewaitingroomevent(struct script_state *st);
int buildin_getwaitingroomstate(struct script_state *st);
int buildin_warpwaitingpc(struct script_state *st);
int buildin_attachrid(struct script_state *st);
int buildin_detachrid(struct script_state *st);
int buildin_isloggedin(struct script_state *st);
int buildin_setmapflagnosave(struct script_state *st);
int buildin_setmapflag(struct script_state *st);
int buildin_removemapflag(struct script_state *st);
int buildin_pvpon(struct script_state *st);
int buildin_pvpoff(struct script_state *st);
int buildin_gvgon(struct script_state *st);
int buildin_gvgoff(struct script_state *st);
int buildin_emotion(struct script_state *st);
int buildin_maprespawnguildid(struct script_state *st);
int buildin_agitstart(struct script_state *st);		// <Agit>
int buildin_agitend(struct script_state *st);
int buildin_agitcheck(struct script_state *st);  // <Agitcheck>
int buildin_flagemblem(struct script_state *st);		// Flag Emblem
int buildin_getcastlename(struct script_state *st);
int buildin_getcastledata(struct script_state *st);
int buildin_setcastledata(struct script_state *st);
int buildin_requestguildinfo(struct script_state *st);
int buildin_getequipcardcnt(struct script_state *st);
int buildin_successremovecards(struct script_state *st);
int buildin_failedremovecards(struct script_state *st);
int buildin_marriage(struct script_state *st);
int buildin_wedding_effect(struct script_state *st);
int buildin_divorce(struct script_state *st);
int buildin_ispartneron(struct script_state *st); // MouseJstr
int buildin_getpartnerid(struct script_state *st); // MouseJstr
int buildin_getchildid(struct script_state *st); // Skotlex
int buildin_getmotherid(struct script_state *st); // Lupus
int buildin_getfatherid(struct script_state *st); // Lupus
int buildin_warppartner(struct script_state *st); // MouseJstr
int buildin_getitemname(struct script_state *st);
int buildin_getitemslots(struct script_state *st);
int buildin_makepet(struct script_state *st);
int buildin_getexp(struct script_state *st);
int buildin_getinventorylist(struct script_state *st);
int buildin_getskilllist(struct script_state *st);
int buildin_clearitem(struct script_state *st);
int buildin_classchange(struct script_state *st);
int buildin_misceffect(struct script_state *st);
int buildin_soundeffect(struct script_state *st);
int buildin_soundeffectall(struct script_state *st);
int buildin_setcastledata(struct script_state *st);
int buildin_mapwarp(struct script_state *st);
int buildin_inittimer(struct script_state *st);
int buildin_stoptimer(struct script_state *st);
int buildin_cmdothernpc(struct script_state *st);
int buildin_mobcount(struct script_state *st);
int buildin_strmobinfo(struct script_state *st); // Script for displaying mob info [Valaris]
int buildin_guardian(struct script_state *st); // Script for displaying mob info [Valaris]
int buildin_guardianinfo(struct script_state *st); // Script for displaying mob info [Valaris]
int buildin_petskillbonus(struct script_state *st); // petskillbonus [Valaris]
int buildin_petrecovery(struct script_state *st); // pet skill for curing status [Valaris]
int buildin_petloot(struct script_state *st); // pet looting [Valaris]
int buildin_petheal(struct script_state *st); // pet healing [Valaris]
//int buildin_petmag(struct script_state *st); // pet magnificat [Valaris]
int buildin_petskillattack(struct script_state *st); // pet skill attacks [Skotlex]
int buildin_petskillattack2(struct script_state *st); // pet skill attacks [Skotlex]
int buildin_petskillsupport(struct script_state *st); // pet support skill [Valaris]
int buildin_skilleffect(struct script_state *st); // skill effects [Celest]
int buildin_npcskilleffect(struct script_state *st); // skill effects for npcs [Valaris]
int buildin_specialeffect(struct script_state *st); // special effect script [Valaris]
int buildin_specialeffect2(struct script_state *st); // special effect script [Valaris]
int buildin_nude(struct script_state *st); // nude [Valaris]
int buildin_atcommand(struct script_state *st); // [MouseJstr]
int buildin_charcommand(struct script_state *st); // [MouseJstr]
int buildin_movenpc(struct script_state *st); // [MouseJstr]
int buildin_message(struct script_state *st); // [MouseJstr]
int buildin_npctalk(struct script_state *st); // [Valaris]
int buildin_hasitems(struct script_state *st); // [Valaris]
int buildin_getlook(struct script_state *st);	//Lorky [Lupus]
int buildin_getsavepoint(struct script_state *st);	//Lorky [Lupus]
int buildin_npcspeed(struct script_state *st); // [Valaris]
int buildin_npcwalkto(struct script_state *st); // [Valaris]
int buildin_npcstop(struct script_state *st); // [Valaris]
int buildin_getmapxy(struct script_state *st);  //get map position for player/npc/pet/mob by Lorky [Lupus]
int buildin_checkoption1(struct script_state *st); // [celest]
int buildin_checkoption2(struct script_state *st); // [celest]
int buildin_guildgetexp(struct script_state *st); // [celest]
int buildin_guildchangegm(struct script_state *st); // [Skotlex]
int buildin_skilluseid(struct script_state *st); // originally by Qamera [celest]
int buildin_skillusepos(struct script_state *st); // originally by Qamera [celest]
int buildin_logmes(struct script_state *st); // [Lupus]
int buildin_summon(struct script_state *st); // [celest]
int buildin_isnight(struct script_state *st); // [celest]
int buildin_isday(struct script_state *st); // [celest]
int buildin_isequipped(struct script_state *st); // [celest]
int buildin_isequippedcnt(struct script_state *st); // [celest]
int buildin_cardscnt(struct script_state *st); // [Lupus]
int buildin_getrefine(struct script_state *st); // [celest]
int buildin_adopt(struct script_state *st);
int buildin_night(struct script_state *st);
int buildin_day(struct script_state *st);
int buildin_getusersname(struct script_state *st); //jA commands added [Lupus]
int buildin_dispbottom(struct script_state *st);
int buildin_recovery(struct script_state *st);
int buildin_getpetinfo(struct script_state *st);
int buildin_checkequipedcard(struct script_state *st);
int buildin_globalmes(struct script_state *st);
int buildin_jump_zero(struct script_state *st);
int buildin_select(struct script_state *st);
int buildin_getmapmobs(struct script_state *st); //jA addition end
int buildin_unequip(struct script_state *st); // unequip [Spectre]
int buildin_getstrlen(struct script_state *st); //strlen [valaris]
int buildin_charisalpha(struct script_state *st);//isalpha [valaris]
int buildin_fakenpcname(struct script_state *st); // [Lance]
int buildin_compare(struct script_state *st); // Lordalfa, to bring strstr to Scripting Engine
int buildin_getiteminfo(struct script_state *st); //[Lupus] returns Items Buy / sell Price, etc info
int buildin_getequipcardid(struct script_state *st); //[Lupus] returns card id from quipped item card slot N
// [zBuffer] List of mathematics commands --->
int buildin_sqrt(struct script_state *st);
int buildin_pow(struct script_state *st);
int buildin_distance(struct script_state *st);
// <--- [zBuffer] List of mathematics commands
// [zBuffer] List of dynamic var commands --->
int buildin_getd(struct script_state *st);
int buildin_setd(struct script_state *st);
// <--- [zBuffer] List of dynamic var commands
int buildin_petstat(struct script_state *st); // [Lance] Pet Stat Rq: Dubby
int buildin_callshop(struct script_state *st); // [Skotlex]
void push_val(struct script_stack *stack,int type,int val);
int run_func(struct script_state *st);

int mapreg_setreg(int num,int val);
int mapreg_setregstr(int num,const char *str);

int buildin_setitemscript(struct script_state *st);
int buildin_disguise(struct script_state *st);
int buildin_undisguise(struct script_state *st);
int buildin_getmonsterinfo(struct script_state *st); // [Lupus]

#ifdef PCRE_SUPPORT
int buildin_defpattern(struct script_state *st); // MouseJstr
int buildin_activatepset(struct script_state *st); // MouseJstr
int buildin_deactivatepset(struct script_state *st); // MouseJstr
int buildin_deletepset(struct script_state *st); // MouseJstr
#endif

struct {
	int (*func)(struct script_state *);
	char *name;
	char *arg;
} buildin_func[]={
	{buildin_axtoi,"axtoi","s"},
#ifndef TXT_ONLY
	{buildin_query_sql, "query_sql", "s*"},
	{buildin_escape_sql, "escape_sql", "s"},
#endif
	{buildin_atoi,"atoi","s"},
	{buildin_mes,"show","s"},
	{buildin_next,"next",""},
	{buildin_close,"close",""},
	{buildin_close2,"close2",""},
	{buildin_menu,"menu","*"},
	{buildin_menu2,"menu2","*"}, //by Mehah
	{buildin_goto,"goto","l"},
	{buildin_callsub,"callsub","i*"},
	{buildin_callfunc,"callfunc","s*"},
	{buildin_return,"return","*"},
	{buildin_getarg,"getarg","i"},
	{buildin_jobchange,"setjob","i*"},
	{buildin_input,"digit","*"},
	{buildin_warp,"warp","sii"},
	{buildin_areawarp,"areawarp","siiiisii"},
	{buildin_warpchar,"warpchar","siii"}, // [LuzZza]
	{buildin_warpparty,"warpparty","siii"}, // [Fredzilla]
	{buildin_warpguild,"warpguild","siii"}, // [Fredzilla]
	{buildin_setlook,"setlook","ii"},
	{buildin_set,"set","ii"},
	{buildin_setarray,"setarray","ii*"},
	{buildin_cleararray,"cleararray","iii"},
	{buildin_copyarray,"copyarray","iii"},
	{buildin_getarraysize,"getarraysize","i"},
	{buildin_deletearray,"deletearray","ii"},
	{buildin_getelementofarray,"getelementofarray","ii"},
	{buildin_getitem,"additem","ii**"},
	{buildin_getitem2,"additem2","iiiiiiiii*"},
	{buildin_getnameditem,"getnameditem","is"},
	{buildin_grouprandomitem,"groupranditem","i"},
	{buildin_makeitem,"makeitem","iisii"},
	{buildin_delitem,"delitem","ii"},
	{buildin_delitem2,"delitem2","iiiiiiiii"},
	{buildin_enableitemuse,"enable_items",""},
	{buildin_disableitemuse,"disable_items",""},
	{buildin_cutin,"cutin","si"},
	{buildin_cutincard,"cutincard","i"},
	{buildin_viewpoint,"viewpoint","iiiii"},
	{buildin_heal,"heal","ii"},
	{buildin_itemheal,"itemheal","ii"},
	{buildin_percentheal,"percentheal","ii"},
	{buildin_rand,"rand","i*"},
	{buildin_countitem,"countitem","i"},
	{buildin_countitem2,"countitem2","iiiiiiii"},
	{buildin_checkweight,"checkweight","ii"},
	{buildin_readparam,"readparam","i*"},
	{buildin_getcharid,"getcharid","i*"},
	{buildin_getpartyname,"getpartyname","i"},
	{buildin_getpartymember,"getpartymember","i*"},
	{buildin_getguildname,"getguildname","i"},
	{buildin_getguildmaster,"getguildmaster","i"},
	{buildin_getguildmasterid,"getguildmasterid","i"},
	{buildin_strcharinfo,"strcharinfo","i"},
	{buildin_getequipid,"getequipid","i"},
	{buildin_getequipname,"getequipname","i"},
	{buildin_getbrokenid,"getbrokenid","i"}, // [Valaris]
	{buildin_repair,"repair","i"}, // [Valaris]
	{buildin_getequipisequiped,"getequipisequiped","i"},
	{buildin_getequipisenableref,"getequipisenableref","i"},
	{buildin_getequipisidentify,"getequipisidentify","i"},
	{buildin_getequiprefinerycnt,"getequiprefinerycnt","i"},
	{buildin_getequipweaponlv,"getequipweaponlv","i"},
	{buildin_getequippercentrefinery,"getequippercentrefinery","i"},
	{buildin_successrefitem,"successrefitem","i"},
	{buildin_failedrefitem,"failedrefitem","i"},
	{buildin_statusup,"statusup","i"},
	{buildin_statusup2,"statusup2","ii"},
	{buildin_bonus,"bonus","ii"},
	{buildin_bonus2,"bonus2","iii"},
	{buildin_bonus3,"bonus3","iiii"},
	{buildin_bonus4,"bonus4","iiiii"},
	{buildin_skill,"skill","ii*"},
	{buildin_addtoskill,"addtoskill","ii*"}, // [Valaris]
	{buildin_guildskill,"guildskill","ii"},
	{buildin_getskilllv,"getskilllv","i"},
	{buildin_getgdskilllv,"getgdskilllv","ii"},
	{buildin_basicskillcheck,"basicskillcheck","*"},
	{buildin_getgmlevel,"getgmlevel","*"},
	{buildin_end,"end",""},
//	{buildin_end,"break",""}, this might confuse advanced scripting support [Eoe]
	{buildin_checkoption,"checkoption","i"},
	{buildin_setoption,"setoption","i"},
	{buildin_setcart,"setcart",""},
	{buildin_checkcart,"checkcart","*"},		//fixed by Lupus (added '*')
	{buildin_setfalcon,"setfalcon",""},
	{buildin_checkfalcon,"checkfalcon","*"},	//fixed by Lupus (fixed wrong pointer, added '*')
	{buildin_setriding,"setriding",""},
	{buildin_checkriding,"checkriding","*"},	//fixed by Lupus (fixed wrong pointer, added '*')
	{buildin_savepoint,"save","sii"},
	{buildin_savepoint,"savepoint","sii"},
	{buildin_gettimetick,"gettimetick","i"},
	{buildin_gettime,"gettime","i"},
	{buildin_gettimestr,"gettimestr","si"},
	{buildin_openstorage,"openstorage",""},
	{buildin_guildopenstorage,"guildopenstorage","*"},
	{buildin_itemskill,"itemskill","iis"},
	{buildin_produce,"produce","i"},
	{buildin_monster,"monster","siisii*"},
	{buildin_areamonster,"areamonster","siiiisii*"},
	{buildin_killmonster,"killmonster","ss"},
	{buildin_killmonsterall,"killmonsterall","s"},
	{buildin_clone,"clone","siisi*"},
	{buildin_doevent,"doevent","s"},
	{buildin_donpcevent,"donpcevent","s"},
	{buildin_addtimer,"addtimer","is"},
	{buildin_deltimer,"deltimer","s"},
	{buildin_addtimercount,"addtimercount","si"},
	{buildin_initnpctimer,"initnpctimer","*"},
	{buildin_stopnpctimer,"stopnpctimer","*"},
	{buildin_startnpctimer,"startnpctimer","*"},
	{buildin_setnpctimer,"setnpctimer","*"},
	{buildin_getnpctimer,"getnpctimer","i*"},
	{buildin_attachnpctimer,"attachnpctimer","*"}, // attached the player id to the npc timer [Celest]
	{buildin_detachnpctimer,"detachnpctimer","*"}, // detached the player id from the npc timer [Celest]
	{buildin_playerattached,"playerattached",""}, // returns id of the current attached player. [Skotlex]
	{buildin_announce,"announce","si*"},
	{buildin_mapannounce,"mapannounce","ssi*"},
	{buildin_areaannounce,"areaannounce","siiiisi*"},
	{buildin_getusers,"getusers","i"},
	{buildin_getmapusers,"getmapusers","s"},
	{buildin_getareausers,"getareausers","siiii"},
	{buildin_getareadropitem,"getareadropitem","siiiii"},
	{buildin_enablenpc,"enablenpc","s"},
	{buildin_disablenpc,"disablenpc","s"},
	{buildin_enablearena,"enablearena",""},		// Added by RoVeRT
	{buildin_disablearena,"disablearena",""},	// Added by RoVeRT
	{buildin_hideoffnpc,"hideoffnpc","s"},
	{buildin_hideonnpc,"hideonnpc","s"},
	{buildin_sc_start,"sc_start","iii*"},
	{buildin_sc_start2,"sc_start2","iiii*"},
	{buildin_sc_start4,"sc_start4","iiiiii*"},
	{buildin_sc_end,"sc_end","i"},
	{buildin_getscrate,"getscrate","ii*"},
	{buildin_debugmes,"debugmes","s"},
	{buildin_catchpet,"pet","i"},
	{buildin_birthpet,"bpet",""},
	{buildin_resetlvl,"resetlvl","i"},
	{buildin_resetstatus,"resetstatus",""},
	{buildin_resetskill,"resetskill",""},
	{buildin_skillpointcount,"skillpointcount",""},
	{buildin_changebase,"setbase","i"},
	{buildin_changesex,"setsex",""},
	{buildin_waitingroom,"waitingroom","si*"},
	{buildin_warpwaitingpc,"warpwaitingpc","sii"},
	{buildin_delwaitingroom,"delwaitingroom","*"},
	{buildin_enablewaitingroomevent,"enablewaitingroomevent","*"},
	{buildin_disablewaitingroomevent,"disablewaitingroomevent","*"},
	{buildin_getwaitingroomstate,"getwaitingroomstate","i*"},
	{buildin_warpwaitingpc,"warpwaitingpc","sii*"},
	{buildin_attachrid,"attachrid","i"},
	{buildin_detachrid,"detachrid",""},
	{buildin_isloggedin,"isloggedin","i"},
	{buildin_setmapflagnosave,"setmapflagnosave","ssii"},
	{buildin_setmapflag,"setmapflag","si"},
	{buildin_removemapflag,"removemapflag","si"},
	{buildin_pvpon,"pvpon","s"},
	{buildin_pvpoff,"pvpoff","s"},
	{buildin_gvgon,"gvgon","s"},
	{buildin_gvgoff,"gvgoff","s"},
	{buildin_emotion,"emotion","i*"},
	{buildin_maprespawnguildid,"maprespawnguildid","sii"},
	{buildin_agitstart,"agitstart",""},	// <Agit>
	{buildin_agitend,"agitend",""},
	{buildin_agitcheck,"agitcheck","i"},   // <Agitcheck>
	{buildin_flagemblem,"flagemblem","i"},	// Flag Emblem
	{buildin_getcastlename,"getcastlename","s"},
	{buildin_getcastledata,"getcastledata","si*"},
	{buildin_setcastledata,"setcastledata","sii"},
	{buildin_requestguildinfo,"requestguildinfo","i*"},
	{buildin_getequipcardcnt,"getequipcardcnt","i"},
	{buildin_successremovecards,"successremovecards","i"},
	{buildin_failedremovecards,"failedremovecards","ii"},
	{buildin_marriage,"marriage","s"},
	{buildin_wedding_effect,"wedding",""},
	{buildin_divorce,"divorce",""},
	{buildin_ispartneron,"ispartneron",""},
	{buildin_getpartnerid,"getpartnerid",""},
	{buildin_getchildid,"getchildid",""},
	{buildin_getmotherid,"getmotherid",""},
	{buildin_getfatherid,"getfatherid",""},
	{buildin_warppartner,"warppartner","sii"},
	{buildin_getitemname,"getitemname","i"},
	{buildin_getitemslots,"getitemslots","i"},
	{buildin_makepet,"makepet","i"},
	{buildin_getexp,"getexp","ii"},
	{buildin_getinventorylist,"getinventorylist",""},
	{buildin_getskilllist,"getskilllist",""},
	{buildin_clearitem,"clearitem",""},
	{buildin_classchange,"classchange","ii"},
	{buildin_misceffect,"misceffect","i"},
	{buildin_soundeffect,"soundeffect","si"},
	{buildin_soundeffectall,"soundeffectall","*"},	// SoundEffectAll [Codemaster]
	{buildin_strmobinfo,"strmobinfo","ii"},	// display mob data [Valaris]
	{buildin_guardian,"guardian","siisii*i"},	// summon guardians
	{buildin_guardianinfo,"guardianinfo","i"},	// display guardian data [Valaris]
	{buildin_petskillbonus,"petskillbonus","iiii"}, // [Valaris]
	{buildin_petrecovery,"petrecovery","ii"}, // [Valaris]
	{buildin_petloot,"petloot","i"}, // [Valaris]
	{buildin_petheal,"petheal","iiii"}, // [Valaris]
//	{buildin_petmag,"petmag","iiii"}, // [Valaris]
	{buildin_petskillattack,"petskillattack","iiii"}, // [Skotlex]
	{buildin_petskillattack2,"petskillattack2","iiiii"}, // [Valaris]
	{buildin_petskillsupport,"petskillsupport","iiiii"}, // [Skotlex]
	{buildin_skilleffect,"skilleffect","ii"}, // skill effect [Celest]
	{buildin_npcskilleffect,"npcskilleffect","iiii"}, // npc skill effect [Valaris]
	{buildin_specialeffect,"specialeffect","i"}, // npc skill effect [Valaris]
	{buildin_specialeffect2,"specialeffect2","i"}, // skill effect on players[Valaris]
	{buildin_nude,"nude",""}, // nude command [Valaris]
	{buildin_mapwarp,"mapwarp","ssii"},		// Added by RoVeRT
	{buildin_inittimer,"inittimer",""},
	{buildin_stoptimer,"stoptimer",""},
	{buildin_cmdothernpc,"cmdothernpc","ss"},
	{buildin_atcommand,"atcommand","*"}, // [MouseJstr]
	{buildin_charcommand,"charcommand","*"}, // [MouseJstr]
//	{buildin_movenpc,"movenpc","siis"}, // [MouseJstr]
	{buildin_message,"message","s*"}, // [MouseJstr]
	{buildin_npctalk,"npctalk","*"}, // [Valaris]
	{buildin_hasitems,"hasitems","*"}, // [Valaris]
	{buildin_mobcount,"mobcount","ss"},
	{buildin_getlook,"getlook","i"},
	{buildin_getsavepoint,"getsavepoint","i"},
	{buildin_npcspeed,"npcspeed","i"}, // [Valaris]
	{buildin_npcwalkto,"npcwalkto","ii"}, // [Valaris]
	{buildin_npcstop,"npcstop",""}, // [Valaris]
	{buildin_getmapxy,"getmapxy","siii*"},	//by Lorky [Lupus]
	{buildin_checkoption1,"checkoption1","i"},
	{buildin_checkoption2,"checkoption2","i"},
	{buildin_guildgetexp,"guildgetexp","i"},
	{buildin_guildchangegm,"guildchangegm","is"},
	{buildin_skilluseid,"skilluseid","ii"}, // originally by Qamera [Celest]
	{buildin_skilluseid,"doskill","ii"}, // since a lot of scripts would already use 'doskill'...
	{buildin_skillusepos,"skillusepos","iiii"}, // [Celest]
	{buildin_logmes,"logmes","s"}, //this command actls as MES but rints info into LOG file either SQL/TXT [Lupus]
	{buildin_summon,"summon","si*"}, // summons a slave monster [Celest]
	{buildin_isnight,"isnight",""}, // check whether it is night time [Celest]
	{buildin_isday,"isday",""}, // check whether it is day time [Celest]
	{buildin_isequipped,"isequipped","i*"}, // check whether another item/card has been equipped [Celest]
	{buildin_isequippedcnt,"isequippedcnt","i*"}, // check how many items/cards are being equipped [Celest]
	{buildin_cardscnt,"cardscnt","i*"}, // check how many items/cards are being equipped in the same arm [Lupus]
	{buildin_getrefine,"getrefine","*"}, // returns the refined number of the current item, or an item with index specified [celest]
	{buildin_adopt,"adopt","sss"}, // allows 2 parents to adopt a child
	{buildin_night,"night",""}, // sets the server to night time
	{buildin_day,"day",""}, // sets the server to day time
#ifdef PCRE_SUPPORT
        {buildin_defpattern, "defpattern", "iss"}, // Define pattern to listen for [MouseJstr]
        {buildin_activatepset, "activatepset", "i"}, // Activate a pattern set [MouseJstr]
        {buildin_deactivatepset, "deactivatepset", "i"}, // Deactive a pattern set [MouseJstr]
        {buildin_deletepset, "deletepset", "i"}, // Delete a pattern set [MouseJstr]
#endif
	{buildin_dispbottom,"dispbottom","s"}, //added from jA [Lupus]
	{buildin_getusersname,"getusersname","*"},
	{buildin_recovery,"recovery",""},
	{buildin_getpetinfo,"getpetinfo","i"},
	{buildin_checkequipedcard,"checkequipedcard","i"},
	{buildin_jump_zero,"jump_zero","ii"}, //for future jA script compatibility
	{buildin_select,"select","*"}, //for future jA script compatibility
	{buildin_globalmes,"globalmes","s*"},
	{buildin_getmapmobs,"getmapmobs","s"}, //end jA addition
	{buildin_unequip,"unequip","i"}, // unequip command [Spectre]
	{buildin_getstrlen,"getstrlen","s"}, //strlen [Valaris]
	{buildin_charisalpha,"charisalpha","si"}, //isalpha [Valaris]
	{buildin_fakenpcname,"fakenpcname","ssi"}, // [Lance]
	{buildin_compare,"compare","ss"}, // Lordalfa - To bring strstr to scripting Engine.
	{buildin_getiteminfo,"getiteminfo","ii"}, //[Lupus] returns Items Buy / sell Price, etc info
	{buildin_getequipcardid,"getequipcardid","ii"}, //[Lupus] returns CARD ID or other info from CARD slot N of equipped item
	// [zBuffer] List of mathematics commands --->
	{buildin_sqrt,"sqrt","i"},
	{buildin_pow,"pow","ii"},
	{buildin_distance,"distance","iiii"},
	// <--- [zBuffer] List of mathematics commands
	// [zBuffer] List of dynamic var commands --->
	{buildin_getd,"getd","*"},
	{buildin_setd,"setd","*"},
	// <--- [zBuffer] List of dynamic var commands
	{buildin_petstat,"petstat","i"},
	{buildin_callshop,"callshop","si"}, // [Skotlex]
	{buildin_setitemscript,"setitemscript","is"}, //Set NEW item bonus script. Lupus
	{buildin_disguise,"disguise","i"}, //disguise player. Lupus
	{buildin_undisguise,"undisguise","i"}, //undisguise player. Lupus
	{buildin_getmonsterinfo,"getmonsterinfo","ii"}, //Lupus
	{NULL,NULL,NULL},
};

enum {
	C_NOP,C_POS,C_INT,C_PARAM,C_FUNC,C_STR,C_CONSTSTR,C_ARG,
	C_NAME,C_EOL, C_RETINFO,
	C_USERFUNC, C_USERFUNC_POS, // user defined functions

	C_LOR,C_LAND,C_LE,C_LT,C_GE,C_GT,C_EQ,C_NE,   //operator
	C_XOR,C_OR,C_AND,C_ADD,C_SUB,C_MUL,C_DIV,C_MOD,C_NEG,C_LNOT,C_NOT,C_R_SHIFT,C_L_SHIFT
};

//Reports on the console the src of an script error.
static void report_src(struct script_state *st) {
	struct block_list *bl;
	if (!st->oid) return; //Can't report source.
	bl = map_id2bl(st->oid);
	if (!bl) return;
	switch (bl->type) {
		case BL_NPC:
			if (bl->m >=0)
				ShowDebug("Source (NPC): %s at %s (%d,%d)\n", ((struct npc_data *)bl)->name, map[bl->m].name, bl->x, bl->y);
			else
				ShowDebug("Source (NPC): %s (invisible/not on a map)\n", ((struct npc_data *)bl)->name);

		break;
		default:
			if (bl->m >=0)
				ShowDebug("Source (Non-NPC): type %d at %s (%d,%d)\n", bl->type, map[bl->m].name, bl->x, bl->y);
			else
				ShowDebug("Source (Non-NPC): type %d (invisible/not on a map)\n", bl->type);
		break;
	}
}
/*==========================================
 * 文字列のハッシュを計算
 *------------------------------------------
 */
static int calc_hash(const unsigned char *p)
{
	int h=0;
	while(*p){
		h=(h<<1)+(h>>3)+(h>>5)+(h>>8);
		h+=*p++;
	}
	return h&15;
}

/*==========================================
 * str_dataの中に名前があるか検索する
 *------------------------------------------
 */
// 既存のであれば番号、無ければ-1
static int search_str(const unsigned char *p)
{
	int i;
	i=str_hash[calc_hash(p)];
	while(i){
		if(strcmp(str_buf+str_data[i].str,(char *) p)==0){
			return i;
		}
		i=str_data[i].next;
	}
	return -1;
}

/*==========================================
 * str_dataに名前を登録
 *------------------------------------------
 */
// 既存のであれば番号、無ければ登録して新規番号
static int add_str(const unsigned char *p)
{
	int i;
	char *lowcase;

	lowcase=aStrdup((char *) p);
	for(i=0;lowcase[i];i++)
		lowcase[i]=tolower(lowcase[i]);
	if((i=search_str((unsigned char *) lowcase))>=0){
		aFree(lowcase);
		return i;
	}
	aFree(lowcase);

	i=calc_hash(p);
	if(str_hash[i]==0){
		str_hash[i]=str_num;
	} else {
		i=str_hash[i];
		for(;;){
			if(strcmp(str_buf+str_data[i].str,(char *) p)==0){
				return i;
			}
			if(str_data[i].next==0)
				break;
			i=str_data[i].next;
		}
		str_data[i].next=str_num;
	}
	if(str_num>=str_data_size){
		str_data_size+=128;
		str_data=(struct str_data_struct *) aRealloc(str_data,sizeof(str_data[0])*str_data_size);
		memset(str_data + (str_data_size - 128), '\0', 128);
	}
	while(str_pos+(int)strlen((char *) p)+1>=str_size){
		str_size+=256;
		str_buf=(char *)aRealloc(str_buf,str_size);
		memset(str_buf + (str_size - 256), '\0', 256);
	}
	strcpy(str_buf+str_pos, (char *) p);
	str_data[str_num].type=C_NOP;
	str_data[str_num].str=str_pos;
	str_data[str_num].next=0;
	str_data[str_num].func=NULL;
	str_data[str_num].backpatch=-1;
	str_data[str_num].label=-1;
	str_pos+=(int)strlen( (char *) p)+1;
	return str_num++;
}


/*==========================================
 * スクリプトバッファサイズの確認と拡張
 *------------------------------------------
 */
static void check_script_buf(int size)
{
	if(script_pos+size>=script_size){
		script_size+=SCRIPT_BLOCK_SIZE;
		script_buf=(unsigned char *)aRealloc(script_buf,script_size);
		memset(script_buf + script_size - SCRIPT_BLOCK_SIZE, '\0',
			SCRIPT_BLOCK_SIZE);
	}
}

/*==========================================
 * スクリプトバッファに１バイト書き込む
 *------------------------------------------
 */
static void add_scriptb(int a)
{
	check_script_buf(1);
	script_buf[script_pos++]=a;
}

/*==========================================
 * スクリプトバッファにデータタイプを書き込む
 *------------------------------------------
 */
static void add_scriptc(int a)
{
	while(a>=0x40){
		add_scriptb((a&0x3f)|0x40);
		a=(a-0x40)>>6;
	}
	add_scriptb(a&0x3f);
}

/*==========================================
 * スクリプトバッファに整数を書き込む
 *------------------------------------------
 */
static void add_scripti(int a)
{
	while(a>=0x40){
		add_scriptb(a|0xc0);
		a=(a-0x40)>>6;
	}
	add_scriptb(a|0x80);
}

/*==========================================
 * スクリプトバッファにラベル/変数/関数を書き込む
 *------------------------------------------
 */
// 最大16Mまで
static void add_scriptl(int l)
{
	int backpatch = str_data[l].backpatch;

	switch(str_data[l].type){
	case C_POS:
	case C_USERFUNC_POS:
		add_scriptc(C_POS);
		add_scriptb(str_data[l].label);
		add_scriptb(str_data[l].label>>8);
		add_scriptb(str_data[l].label>>16);
		break;
	case C_NOP:
	case C_USERFUNC:
		// ラベルの可能性があるのでbackpatch用データ埋め込み
		add_scriptc(C_NAME);
		str_data[l].backpatch=script_pos;
		add_scriptb(backpatch);
		add_scriptb(backpatch>>8);
		add_scriptb(backpatch>>16);
		break;
	case C_INT:
		add_scripti(str_data[l].val);
		break;
	default:
		// もう他の用途と確定してるので数字をそのまま
		add_scriptc(C_NAME);
		add_scriptb(l);
		add_scriptb(l>>8);
		add_scriptb(l>>16);
		break;
	}
}

/*==========================================
 * ラベルを解決する
 *------------------------------------------
 */
void set_label(int l,int pos)
{
	int i,next;

	str_data[l].type=(str_data[l].type == C_USERFUNC ? C_USERFUNC_POS : C_POS);
	str_data[l].label=pos;
	for(i=str_data[l].backpatch;i>=0 && i!=0x00ffffff;){
		next=(*(int*)(script_buf+i)) & 0x00ffffff;
		script_buf[i-1]=(str_data[l].type == C_USERFUNC ? C_USERFUNC_POS : C_POS);
		script_buf[i]=pos;
		script_buf[i+1]=pos>>8;
		script_buf[i+2]=pos>>16;
		i=next;
	}
}

/*==========================================
 * スペース/コメント読み飛ばし
 *------------------------------------------
 */
static unsigned char *skip_space(unsigned char *p)
{
	while(1){
		while(isspace(*p))
			p++;
		if(p[0]=='/' && p[1]=='/'){
			while(*p && *p!='\n')
				p++;
		} else if(p[0]=='/' && p[1]=='*'){
			p++;
			while(*p && (p[-1]!='*' || p[0]!='/'))
				p++;
			if(*p) p++;
		} else
			break;
	}
	return p;
}

/*==========================================
 * １単語スキップ
 *------------------------------------------
 */
static unsigned char *skip_word(unsigned char *p)
{
	// prefix
	if(*p=='$') p++;	// MAP鯖内共有変数用
	if(*p=='@') p++;	// 一時的変数用(like weiss)
	if(*p=='#') p++;	// account変数用
	if(*p=='#') p++;	// ワールドaccount変数用

	while(isalnum(*p)||*p=='_'|| *p>=0x81)
		if(*p>=0x81 && p[1]){
			p+=2;
		} else
			p++;

	// postfix
	if(*p=='$') p++;	// 文字列変数

	return p;
}

static unsigned char *startptr;
static int startline;

/*==========================================
 * エラーメッセージ出力
 *------------------------------------------
 */
static void disp_error_message(const char *mes,const unsigned char *pos)
{
	int line,c=0,i;
	unsigned char *p,*linestart,*lineend;

	for(line=startline,p=startptr;p && *p;line++){
		linestart=p;
		lineend=(unsigned char *) strchr((char *) p,'\n');
		if(lineend){
			c=*lineend;
			*lineend=0;
		}
		if(lineend==NULL || pos<lineend){
			fprintf(stderr, "\r"); //To not printout the error next to the spinner...
			ShowError(" "); //Better error display [Skotlex]
			if (current_file) {
				printf("%s in "CL_WHITE"\'%s\'"CL_RESET" line "CL_WHITE"\'%d\'"CL_RESET" : ", mes, current_file, line);
			} else {
				printf("%s line "CL_WHITE"\'%d\'"CL_RESET" : ", mes, line);
			}
			for(i=0;(linestart[i]!='\r') && (linestart[i]!='\n') && linestart[i];i++){
				if(linestart+i!=pos)
					printf("%c",linestart[i]);
				else
					printf("\'%c\'",linestart[i]);
			}
			printf("\a\n");
			if(lineend)
				*lineend=c;
			return;
		}
		*lineend=c;
		p=lineend+1;
	}
}

/*==========================================
 * 項の解析
 *------------------------------------------
 */
unsigned char* parse_simpleexpr(unsigned char *p)
{
	int i;
	p=skip_space(p);

#ifdef DEBUG_FUNCIN
	if(battle_config.etc_log)
		ShowDebug("parse_simpleexpr %s\n",p);
#endif
	if(*p==';' || *p==','){
		disp_error_message("unexpected expr end",p);
		exit(1);
	}
	if(*p=='('){

		p=parse_subexpr(p+1,-1);
		p=skip_space(p);
		if((*p++)!=')'){
			disp_error_message("unmatch ')'",p);
			exit(1);
		}
	} else if(isdigit(*p) || ((*p=='-' || *p=='+') && isdigit(p[1]))){
		char *np;
		i=strtoul((char *) p,&np,0);
		add_scripti(i);
		p=(unsigned char *) np;
	} else if(*p=='"'){
		add_scriptc(C_STR);
		p++;
		while(*p && *p!='"'){
			if(p[-1]<=0x7e && *p=='\\')
				p++;
			else if(*p=='\n'){
				disp_error_message("unexpected newline @ string",p);
				exit(1);
			}
			add_scriptb(*p++);
		}
		if(!*p){
			disp_error_message("unexpected eof @ string",p);
			exit(1);
		}
		add_scriptb(0);
		p++;	//'"'
	} else {
		int c,l;
		char *p2;
		// label , register , function etc
		if(skip_word(p)==p && !(*p==')' && p[-1]=='(')){
			disp_error_message("unexpected character",p);
			exit(1);
		}

		p2=(char *) skip_word(p);
		c=*p2;	*p2=0;	// 名前をadd_strする
		l=add_str(p);

		parse_cmd=l;	// warn_*_mismatch_paramnumのために必要

		*p2=c;
		p=(unsigned char *) p2;

		if(str_data[l].type!=C_FUNC && c=='['){
			// array(name[i] => getelementofarray(name,i) )
			add_scriptl(search_str((unsigned char *) "getelementofarray"));
			add_scriptc(C_ARG);
			add_scriptl(l);
			p=parse_subexpr(p+1,-1);
			p=skip_space(p);
			if((*p++)!=']'){
				disp_error_message("unmatch ']'",p);
				exit(1);
			}
			add_scriptc(C_FUNC);
		} else if(str_data[l].type == C_USERFUNC || str_data[l].type == C_USERFUNC_POS) {
			add_scriptl(search_str((unsigned char*)"callsub"));
			add_scriptc(C_ARG);
			add_scriptl(l);
		}else
			add_scriptl(l);

	}

#ifdef DEBUG_FUNCIN
	if(battle_config.etc_log)
		ShowDebug("parse_simpleexpr end %s\n",p);
#endif
	return p;
}

/*==========================================
 * 式の解析
 *------------------------------------------
 */
unsigned char* parse_subexpr(unsigned char *p,int limit)
{
	int op,opl,len;
	char *tmpp;

#ifdef DEBUG_FUNCIN
	if(battle_config.etc_log)
		ShowDebug("parse_subexpr %s\n",p);
#endif
	p=skip_space(p);

	if(*p=='-'){
		tmpp=(char *) skip_space((unsigned char *) (p+1));
		if(*tmpp==';' || *tmpp==','){
			add_scriptl(LABEL_NEXTLINE);
			p++;
			return p;
		}
	}
	tmpp=(char *) p;
	if((op=C_NEG,*p=='-') || (op=C_LNOT,*p=='!') || (op=C_NOT,*p=='~')){
		p=parse_subexpr(p+1,8);
		add_scriptc(op);
	} else
		p=parse_simpleexpr(p);
	p=skip_space(p);
	while(((op=C_ADD,opl=6,len=1,*p=='+') ||
		   (op=C_SUB,opl=6,len=1,*p=='-') ||
		   (op=C_MUL,opl=7,len=1,*p=='*') ||
		   (op=C_DIV,opl=7,len=1,*p=='/') ||
		   (op=C_MOD,opl=7,len=1,*p=='%') ||
		   (op=C_FUNC,opl=9,len=1,*p=='(') ||
		   (op=C_LAND,opl=1,len=2,*p=='&' && p[1]=='&') ||
		   (op=C_AND,opl=5,len=1,*p=='&') ||
		   (op=C_LOR,opl=0,len=2,*p=='|' && p[1]=='|') ||
		   (op=C_OR,opl=4,len=1,*p=='|') ||
		   (op=C_XOR,opl=3,len=1,*p=='^') ||
		   (op=C_EQ,opl=2,len=2,*p=='=' && p[1]=='=') ||
		   (op=C_NE,opl=2,len=2,*p=='!' && p[1]=='=') ||
		   (op=C_R_SHIFT,opl=5,len=2,*p=='>' && p[1]=='>') ||
		   (op=C_GE,opl=2,len=2,*p=='>' && p[1]=='=') ||
		   (op=C_GT,opl=2,len=1,*p=='>') ||
		   (op=C_L_SHIFT,opl=5,len=2,*p=='<' && p[1]=='<') ||
		   (op=C_LE,opl=2,len=2,*p=='<' && p[1]=='=') ||
		   (op=C_LT,opl=2,len=1,*p=='<')) && opl>limit){
		p+=len;
		if(op==C_FUNC){
			int i=0,func=parse_cmd;
			const char *plist[128];

			if(str_data[parse_cmd].type == C_FUNC){
				// 通常の関数
				add_scriptc(C_ARG);
			} else if(str_data[parse_cmd].type == C_USERFUNC || str_data[parse_cmd].type == C_USERFUNC_POS) {
				// ユーザー定義関数呼び出し
				parse_cmd = search_str((unsigned char*)"callsub");
				i++;
			} else {
				disp_error_message(
					"expect command, missing function name or calling undeclared function",(unsigned char *) tmpp
				);
				exit(0);
			}
			func=parse_cmd;

			do {
				plist[i]=(char *) p;
				p=parse_subexpr(p,-1);
				p=skip_space(p);
				if(*p==',') p++;
				else if(*p!=')' && script_config.warn_func_no_comma){
					disp_error_message("expect ',' or ')' at func params",p);
				}
				p=skip_space(p);
				i++;
			} while(*p && *p!=')' && i<128);
			plist[i]=(char *) p;
			if(*(p++)!=')'){
				disp_error_message("func request '(' ')'",p);
				exit(1);
			}

			if (str_data[func].type == C_FUNC && script_config.warn_func_mismatch_paramnum) {
				const char *arg = buildin_func[str_data[func].val].arg;
				int j = 0;
				for (; arg[j]; j++) if (arg[j] == '*') break;
				if (!(i <= 1 && j == 0) && ((arg[j] == 0 && i != j) || (arg[j] == '*' && i < j))) {
					disp_error_message("illegal number of parameters",(unsigned char *) (plist[(i<j)?i:j]));
				}
			}
		} else {
			p=parse_subexpr(p,opl);
		}
		add_scriptc(op);
		p=skip_space(p);
	}
#ifdef DEBUG_FUNCIN
	if(battle_config.etc_log)
		ShowDebug("parse_subexpr end %s\n",p);
#endif
	return p;  /* return first untreated operator */
}

/*==========================================
 * 式の評価
 *------------------------------------------
 */
unsigned char* parse_expr(unsigned char *p)
{
#ifdef DEBUG_FUNCIN
	if(battle_config.etc_log)
		ShowDebug("parse_expr %s\n",p);
#endif
	switch(*p){
	case ')': case ';': case ':': case '[': case ']':
	case '}':
		disp_error_message("unexpected char",p);
		exit(1);
	}
	p=parse_subexpr(p,-1);
#ifdef DEBUG_FUNCIN
	if(battle_config.etc_log)
		ShowDebug("parse_expr end %s\n",p);
#endif
	return p;
}

/*==========================================
 * 行の解析
 *------------------------------------------
 */
unsigned char* parse_line(unsigned char *p)
{
	int i=0,cmd;
	const char *plist[128];
	unsigned char *p2;
	char end;

	p=skip_space(p);
	if(*p==';')
		return p + 1;

	p = skip_space(p);
	if(p[0] == '{') {
		syntax.curly[syntax.curly_count].type  = TYPE_NULL;
		syntax.curly[syntax.curly_count].count = -1;
		syntax.curly[syntax.curly_count].index = -1;
		syntax.curly_count++;
		return p + 1;
	} else if(p[0] == '}') {
		return parse_curly_close(p);
	}

	// 構文関連の処理
	p2 = parse_syntax(p);
	if(p2 != NULL) { return p2; }

	// 最初は関数名
	p2=(char *) p;
	p=parse_simpleexpr(p);
	p=skip_space(p);

	if(str_data[parse_cmd].type == C_FUNC){
		// 通常の関数
		add_scriptc(C_ARG);
	} else if(str_data[parse_cmd].type == C_USERFUNC || str_data[parse_cmd].type == C_USERFUNC_POS) {
		// ユーザー定義関数呼び出し
		parse_cmd = search_str((unsigned char*)"callsub");
		i++;
	} else {
		disp_error_message(
			"expect command, missing function name or calling undeclared function", (unsigned char *)p2
		);
//		exit(0);
	}
	cmd=parse_cmd;

	if(parse_syntax_for_flag) {
		end = ')';
	} else {
		end = ';';
	}
	while(p && *p && *p!=end && i<128){
		plist[i]=(char *) p;

		p=parse_expr(p);
		p=skip_space(p);
		// 引数区切りの,処理
		if(*p==',') p++;
		else if(*p!=end && script_config.warn_cmd_no_comma && 0 <= i ){
			if(parse_syntax_for_flag) {
				disp_error_message("expect ',' or ')' at cmd params",p);
			} else {
				disp_error_message("expect ',' or ';' at cmd params",p);
			}
		}
		p=skip_space(p);
		i++;
	}
	plist[i]=(char *) p;
	if(!p || *(p++)!=end){
		if(parse_syntax_for_flag) {
			disp_error_message("need ')'",p);
		} else {
			disp_error_message("need ';'",p);
		}
		exit(1);
	}
	add_scriptc(C_FUNC);

	// if, for , while の閉じ判定
	p = parse_syntax_close(p);

	if( str_data[cmd].type==C_FUNC && script_config.warn_cmd_mismatch_paramnum){
		const char *arg=buildin_func[str_data[cmd].val].arg;
		int j=0;
		for(j=0;arg[j];j++) if(arg[j]=='*')break;
		if( (arg[j]==0 && i!=j) || (arg[j]=='*' && i<j) ){
			disp_error_message("illegal number of parameters",(unsigned char *) (plist[(i<j)?i:j]));
		}
	}


	return p;
}


// { ... } の閉じ処理
unsigned char* parse_curly_close(unsigned char *p) {
	if(syntax.curly_count <= 0) {
		disp_error_message("unexpected string",p);
		return p + 1;
	} else if(syntax.curly[syntax.curly_count-1].type == TYPE_NULL) {
		syntax.curly_count--;
		// if, for , while の閉じ判定
		p = parse_syntax_close(p + 1);
		return p;
	} else if(syntax.curly[syntax.curly_count-1].type == TYPE_SWITCH) {
		// switch() 閉じ判定
		int pos = syntax.curly_count-1;
		unsigned char label[256];
		int l;
		// 一時変数を消す
		sprintf(label,"set $@__SW%x_VAL,0;",syntax.curly[pos].index);
		syntax.curly[syntax.curly_count++].type = TYPE_NULL;
		parse_line(label);
		syntax.curly_count--;

		// 無条件で終了ポインタに移動
		sprintf(label,"goto __SW%x_FIN;",syntax.curly[pos].index);
		syntax.curly[syntax.curly_count++].type = TYPE_NULL;
		parse_line(label);
		syntax.curly_count--;

		// 現在地のラベルを付ける
		sprintf(label,"__SW%x_%x",syntax.curly[pos].index,syntax.curly[pos].count);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);

		if(syntax.curly[pos].flag) {
			// default が存在する
			sprintf(label,"goto __SW%x_DEF;",syntax.curly[pos].index);
			syntax.curly[syntax.curly_count++].type = TYPE_NULL;
			parse_line(label);
			syntax.curly_count--;
		}

		// 終了ラベルを付ける
		sprintf(label,"__SW%x_FIN",syntax.curly[pos].index);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);

		syntax.curly_count--;
		return p+1;
	} else {
		disp_error_message("unexpected string",p);
		return p + 1;
	}
}

// 構文関連の処理
//     break, case, continue, default, do, for, function,
//     if, switch, while をこの内部で処理します。
unsigned char* parse_syntax(unsigned char *p) {
	switch(p[0]) {
	case 'b':
		if(!strncmp(p,"break",5) && !isalpha(*(p + 5))) {
			// break の処理
			char label[256];
			int pos = syntax.curly_count - 1;
			while(pos >= 0) {
				if(syntax.curly[pos].type == TYPE_DO) {
					sprintf(label,"goto __DO%x_FIN;",syntax.curly[pos].index);
					break;
				} else if(syntax.curly[pos].type == TYPE_FOR) {
					sprintf(label,"goto __FR%x_FIN;",syntax.curly[pos].index);
					break;
				} else if(syntax.curly[pos].type == TYPE_WHILE) {
					sprintf(label,"goto __WL%x_FIN;",syntax.curly[pos].index);
					break;
				} else if(syntax.curly[pos].type == TYPE_SWITCH) {
					sprintf(label,"goto __SW%x_FIN;",syntax.curly[pos].index);
					break;
				}
				pos--;
			}
			if(pos < 0) {
				disp_error_message("unexpected 'break'",p);
			} else {
				syntax.curly[syntax.curly_count++].type = TYPE_NULL;
				parse_line(label);
				syntax.curly_count--;
			}
			p = skip_word(p);
			p++;
			// if, for , while の閉じ判定
			p = parse_syntax_close(p + 1);
			return p;
		}
		break;
	case 'c':
		if(!strncmp(p,"case",4) && !isalpha(*(p + 4))) {
			// case の処理
			if(syntax.curly_count <= 0 || syntax.curly[syntax.curly_count - 1].type != TYPE_SWITCH) {
				disp_error_message("unexpected 'case' ",p);
				return p+1;
			} else {
				char *p2;
				char label[256];
				int  l;
				int pos = syntax.curly_count-1;
				if(syntax.curly[pos].count != 1) {
					// FALLTHRU 用のジャンプ
					sprintf(label,"goto __SW%x_%xJ;",syntax.curly[pos].index,syntax.curly[pos].count);
					syntax.curly[syntax.curly_count++].type = TYPE_NULL;
					parse_line(label);
					syntax.curly_count--;

					// 現在地のラベルを付ける
					sprintf(label,"__SW%x_%x",syntax.curly[pos].index,syntax.curly[pos].count);
					l=add_str(label);
					if(str_data[l].label!=-1){
						disp_error_message("dup label ",p);
						exit(1);
					}
					set_label(l,script_pos);
				}
				// switch 判定文
				p = skip_word(p);
				p = skip_space(p);
				p2 = p;
				p = skip_word(p);
				p = skip_space(p);
				if(*p != ':') {
					disp_error_message("expect ':'",p);
					exit(1);
				}
				*p = 0;
				sprintf(label,"if(%s != $@__SW%x_VAL) goto __SW%x_%x;",
					p2,syntax.curly[pos].index,syntax.curly[pos].index,syntax.curly[pos].count+1);
				syntax.curly[syntax.curly_count++].type = TYPE_NULL;
				*p = ':';
				// ２回parse しないとダメ
				p2 = parse_line(label);
				parse_line(p2);
				syntax.curly_count--;
				if(syntax.curly[pos].count != 1) {
					// FALLTHRU 終了後のラベル
					sprintf(label,"__SW%x_%xJ",syntax.curly[pos].index,syntax.curly[pos].count);
					l=add_str(label);
					if(str_data[l].label!=-1){
						disp_error_message("dup label ",p);
						exit(1);
					}
					set_label(l,script_pos);
				}
				// 一時変数を消す
				sprintf(label,"set $@__SW%x_VAL,0;",syntax.curly[pos].index);
				syntax.curly[syntax.curly_count++].type = TYPE_NULL;
				parse_line(label);
				syntax.curly_count--;
				syntax.curly[pos].count++;
			}
			return p + 1;
		} else if(!strncmp(p,"continue",8) && !isalpha(*(p + 8))) {
			// continue の処理
			char label[256];
			int pos = syntax.curly_count - 1;
			while(pos >= 0) {
				if(syntax.curly[pos].type == TYPE_DO) {
					sprintf(label,"goto __DO%x_NXT;",syntax.curly[pos].index);
					syntax.curly[pos].flag = 1; // continue 用のリンク張るフラグ
					break;
				} else if(syntax.curly[pos].type == TYPE_FOR) {
					sprintf(label,"goto __FR%x_NXT;",syntax.curly[pos].index);
					break;
				} else if(syntax.curly[pos].type == TYPE_WHILE) {
					sprintf(label,"goto __WL%x_NXT;",syntax.curly[pos].index);
					break;
				}
				pos--;
			}
			if(pos < 0) {
				disp_error_message("unexpected 'continue'",p);
			} else {
				syntax.curly[syntax.curly_count++].type = TYPE_NULL;
				parse_line(label);
				syntax.curly_count--;
			}
			p = skip_word(p);
			p++;
			// if, for , while の閉じ判定
			p = parse_syntax_close(p + 1);
			return p;
		}
		break;
	case 'd':
		if(!strncmp(p,"default",7) && !isalpha(*(p + 7))) {
			// switch - default の処理
			if(syntax.curly_count <= 0 || syntax.curly[syntax.curly_count - 1].type != TYPE_SWITCH) {
				disp_error_message("unexpected 'delault'",p);
				return p+1;
			} else if(syntax.curly[syntax.curly_count - 1].flag) {
				disp_error_message("dup 'delault'",p);
				return p+1;
			} else {
				char label[256];
				int l;
				int pos = syntax.curly_count-1;
				// 現在地のラベルを付ける
				p = skip_word(p);
				p = skip_space(p);
				if(*p != ':') {
					disp_error_message("need ':'",p);
				}
				p++;
				sprintf(label,"__SW%x_%x",syntax.curly[pos].index,syntax.curly[pos].count);
				l=add_str(label);
				if(str_data[l].label!=-1){
					disp_error_message("dup label ",p);
					exit(1);
				}
				set_label(l,script_pos);

				// 無条件で次のリンクに飛ばす
				sprintf(label,"goto __SW%x_%x;",syntax.curly[pos].index,syntax.curly[pos].count+1);
				syntax.curly[syntax.curly_count++].type = TYPE_NULL;
				parse_line(label);
				syntax.curly_count--;

				// default のラベルを付ける
				sprintf(label,"__SW%x_DEF",syntax.curly[pos].index);
				l=add_str(label);
				if(str_data[l].label!=-1){
					disp_error_message("dup label ",p);
					exit(1);
				}
				set_label(l,script_pos);

				syntax.curly[syntax.curly_count - 1].flag = 1;
				syntax.curly[pos].count++;

				p = skip_word(p);
				return p + 1;
			}
		} else if(!strncmp(p,"do",2) && !isalpha(*(p + 2))) {
			int l;
			char label[256];
			p=skip_word(p);
			p=skip_space(p);

			syntax.curly[syntax.curly_count].type  = TYPE_DO;
			syntax.curly[syntax.curly_count].count = 1;
			syntax.curly[syntax.curly_count].index = syntax.index++;
			syntax.curly[syntax.curly_count].flag  = 0;
			// 現在地のラベル形成する
			sprintf(label,"__DO%x_BGN",syntax.curly[syntax.curly_count].index);
			l=add_str(label);
			if(str_data[l].label!=-1){
				disp_error_message("dup label ",p);
				exit(1);
			}
			set_label(l,script_pos);
			syntax.curly_count++;
			return p;
		}
		break;
	case 'f':
		if(!strncmp(p,"for",3) && !isalpha(*(p + 3))) {
			int l;
			unsigned char label[256];
			int  pos = syntax.curly_count;
			syntax.curly[syntax.curly_count].type  = TYPE_FOR;
			syntax.curly[syntax.curly_count].count = 1;
			syntax.curly[syntax.curly_count].index = syntax.index++;
			syntax.curly[syntax.curly_count].flag  = 0;
			syntax.curly_count++;

			p=skip_word(p);
			p=skip_space(p);

			if(*p != '(') {
				disp_error_message("need '('",p);
				return p+1;
			}
			p++;

			// 初期化文を実行する
			syntax.curly[syntax.curly_count++].type = TYPE_NULL;
			p=parse_line(p);
			syntax.curly_count--;

			// 条件判断開始のラベル形成する
			sprintf(label,"__FR%x_J",syntax.curly[pos].index);
			l=add_str(label);
			if(str_data[l].label!=-1){
				disp_error_message("dup label ",p);
				exit(1);
			}
			set_label(l,script_pos);

			if(*p == ';') {
				// for(;;) のパターンなので必ず真
				;
			} else {
				// 条件が偽なら終了地点に飛ばす
				sprintf(label,"__FR%x_FIN",syntax.curly[pos].index);
				add_scriptl(add_str("jump_zero"));
				add_scriptc(C_ARG);
				p=parse_expr(p);
				p=skip_space(p);
				add_scriptl(add_str(label));
				add_scriptc(C_FUNC);
			}
			if(*p != ';') {
				disp_error_message("need ';'",p);
				return p+1;
			}
			p++;

			// ループ開始に飛ばす
			sprintf(label,"goto __FR%x_BGN;",syntax.curly[pos].index);
			syntax.curly[syntax.curly_count++].type = TYPE_NULL;
			parse_line(label);
			syntax.curly_count--;

			// 次のループへのラベル形成する
			sprintf(label,"__FR%x_NXT",syntax.curly[pos].index);
			l=add_str(label);
			if(str_data[l].label!=-1){
				disp_error_message("dup label ",p);
				exit(1);
			}
			set_label(l,script_pos);

			// 次のループに入る時の処理
			// for 最後の '(' を ';' として扱うフラグ
			parse_syntax_for_flag = 1;
			syntax.curly[syntax.curly_count++].type = TYPE_NULL;
			p=parse_line(p);
			syntax.curly_count--;
			parse_syntax_for_flag = 0;

			// 条件判定処理に飛ばす
			sprintf(label,"goto __FR%x_J;",syntax.curly[pos].index);
			syntax.curly[syntax.curly_count++].type = TYPE_NULL;
			parse_line(label);
			syntax.curly_count--;

			// ループ開始のラベル付け
			sprintf(label,"__FR%x_BGN",syntax.curly[pos].index);
			l=add_str(label);
			if(str_data[l].label!=-1){
				disp_error_message("dup label ",p);
				exit(1);
			}
			set_label(l,script_pos);
			return p;
		} else if(!strncmp(p,"function",8) && !isalpha(*(p + 8))) {
			unsigned char *func_name;
			// function
			p=skip_word(p);
			p=skip_space(p);
			// function - name
			func_name = p;
			p=skip_word(p);
			if(*skip_space(p) == ';') {
				// 関数の宣言 - 名前を登録して終わり
				unsigned char c = *p;
				int l;
				*p = 0;
				l=add_str(func_name);
				*p = c;
				if(str_data[l].type == C_NOP) {
					str_data[l].type = C_USERFUNC;
				}
				return skip_space(p) + 1;
			} else {
				// 関数の中身
				char label[256];
				unsigned char c = *p;
				int l;
				syntax.curly[syntax.curly_count].type  = TYPE_USERFUNC;
				syntax.curly[syntax.curly_count].count = 1;
				syntax.curly[syntax.curly_count].index = syntax.index++;
				syntax.curly[syntax.curly_count].flag  = 0;
				syntax.curly_count++;

				// 関数終了まで飛ばす
				sprintf(label,"goto __FN%x_FIN;",syntax.curly[syntax.curly_count-1].index);
				syntax.curly[syntax.curly_count++].type = TYPE_NULL;
				parse_line(label);
				syntax.curly_count--;

				// 関数名のラベルを付ける
				*p = 0;
				l=add_str(func_name);
				if(str_data[l].type == C_NOP) {
					str_data[l].type = C_USERFUNC;
				}
				if(str_data[l].label!=-1){
					*p=c;
					disp_error_message("dup label ",p);
					exit(1);
				}
				set_label(l,script_pos);
				strdb_put(scriptlabel_db,func_name,(void*)script_pos);	// 外部用label db登録
				*p = c;
				return skip_space(p);
			}
		}
		break;
	case 'i':
		if(!strncmp(p,"if",2) && !isalpha(*(p + 2))) {
			// if() の処理
			char label[256];
			p=skip_word(p);
			p=skip_space(p);

			syntax.curly[syntax.curly_count].type  = TYPE_IF;
			syntax.curly[syntax.curly_count].count = 1;
			syntax.curly[syntax.curly_count].index = syntax.index++;
			syntax.curly[syntax.curly_count].flag  = 0;
			sprintf(label,"__IF%x_%x",syntax.curly[syntax.curly_count].index,syntax.curly[syntax.curly_count].count);
			syntax.curly_count++;
			add_scriptl(add_str("jump_zero"));
			add_scriptc(C_ARG);
			p=parse_expr(p);
			p=skip_space(p);
			add_scriptl(add_str(label));
			add_scriptc(C_FUNC);
			return p;
		}
		break;
	case 's':
		if(!strncmp(p,"switch",6) && !isalpha(*(p + 6))) {
			// switch() の処理
			char label[256];
			syntax.curly[syntax.curly_count].type  = TYPE_SWITCH;
			syntax.curly[syntax.curly_count].count = 1;
			syntax.curly[syntax.curly_count].index = syntax.index++;
			syntax.curly[syntax.curly_count].flag  = 0;
			sprintf(label,"$@__SW%x_VAL",syntax.curly[syntax.curly_count].index);
			syntax.curly_count++;
			add_scriptl(add_str((unsigned char*)"set"));
			add_scriptc(C_ARG);
			add_scriptl(add_str(label));
			p=skip_word(p);
			p=skip_space(p);
			p=parse_expr(p);
			p=skip_space(p);
			if(*p != '{') {
				disp_error_message("need '{'",p);
			}
			add_scriptc(C_FUNC);
			return p + 1;
		}
		break;
	case 'w':
		if(!strncmp(p,"while",5) && !isalpha(*(p + 5))) {
			int l;
			char label[256];
			p=skip_word(p);
			p=skip_space(p);

			syntax.curly[syntax.curly_count].type  = TYPE_WHILE;
			syntax.curly[syntax.curly_count].count = 1;
			syntax.curly[syntax.curly_count].index = syntax.index++;
			syntax.curly[syntax.curly_count].flag  = 0;
			// 条件判断開始のラベル形成する
			sprintf(label,"__WL%x_NXT",syntax.curly[syntax.curly_count].index);
			l=add_str(label);
			if(str_data[l].label!=-1){
				disp_error_message("dup label ",p);
				exit(1);
			}
			set_label(l,script_pos);

			// 条件が偽なら終了地点に飛ばす
			sprintf(label,"__WL%x_FIN",syntax.curly[syntax.curly_count].index);
			add_scriptl(add_str("jump_zero"));
			add_scriptc(C_ARG);
			p=parse_expr(p);
			p=skip_space(p);
			add_scriptl(add_str(label));
			add_scriptc(C_FUNC);
			syntax.curly_count++;
			return p;
		}
		break;
	}
	return NULL;
}

unsigned char* parse_syntax_close(unsigned char *p) {
	// if(...) for(...) hoge(); のように、１度閉じられたら再度閉じられるか確認する
	int flag;

	do {
		p = parse_syntax_close_sub(p,&flag);
	} while(flag);
	return p;
}

// if, for , while , do の閉じ判定
//     flag == 1 : 閉じられた
//     flag == 0 : 閉じられない
unsigned char* parse_syntax_close_sub(unsigned char *p,int *flag) {
	unsigned char label[256];
	int pos = syntax.curly_count - 1;
	int l;
	*flag = 1;

	if(syntax.curly_count <= 0) {
		*flag = 0;
		return p;
	} else if(syntax.curly[pos].type == TYPE_IF) {
		char *p2 = p;
		// if 最終場所へ飛ばす
		sprintf(label,"goto __IF%x_FIN;",syntax.curly[pos].index);
		syntax.curly[syntax.curly_count++].type = TYPE_NULL;
		parse_line(label);
		syntax.curly_count--;

		// 現在地のラベルを付ける
		sprintf(label,"__IF%x_%x",syntax.curly[pos].index,syntax.curly[pos].count);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);

		syntax.curly[pos].count++;
		p = skip_space(p);
		if(!syntax.curly[pos].flag && !strncmp(p,"else",4) && !isalpha(*(p + 4))) {
			// else  or else - if
			p = skip_word(p);
			p = skip_space(p);
			if(!strncmp(p,"if",2) && !isalpha(*(p + 2))) {
				// else - if
				p=skip_word(p);
				p=skip_space(p);
				sprintf(label,"__IF%x_%x",syntax.curly[pos].index,syntax.curly[pos].count);
				add_scriptl(add_str("jump_zero"));
				add_scriptc(C_ARG);
				p=parse_expr(p);
				p=skip_space(p);
				add_scriptl(add_str(label));
				add_scriptc(C_FUNC);
				*flag = 0;
				return p;
			} else {
				// else
				if(!syntax.curly[pos].flag) {
					syntax.curly[pos].flag = 1;
					*flag = 0;
					return p;
				}
			}
		}
		// if 閉じ
		syntax.curly_count--;
		// 最終地のラベルを付ける
		sprintf(label,"__IF%x_FIN",syntax.curly[pos].index);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);
		if(syntax.curly[pos].flag == 1) {
			// このifに対するelseじゃないのでポインタの位置は同じ
			return p2;
		}
		return p;
	} else if(syntax.curly[pos].type == TYPE_DO) {
		int l;
		char label[256];
		unsigned char *p2;

		if(syntax.curly[pos].flag) {
			// 現在地のラベル形成する(continue でここに来る)
			sprintf(label,"__DO%x_NXT",syntax.curly[pos].index);
			l=add_str(label);
			if(str_data[l].label!=-1){
				disp_error_message("dup label ",p);
				exit(1);
			}
			set_label(l,script_pos);
		}

		// 条件が偽なら終了地点に飛ばす
		p = skip_space(p);
		p2 = skip_word(p);
		if(p2 - p != 5 || strncmp("while",p,5)) {
			disp_error_message("need 'while'",p);
		}
		p = p2;

		sprintf(label,"__DO%x_FIN",syntax.curly[pos].index);
		add_scriptl(add_str("jump_zero"));
		add_scriptc(C_ARG);
		p=parse_expr(p);
		p=skip_space(p);
		add_scriptl(add_str(label));
		add_scriptc(C_FUNC);

		// 開始地点に飛ばす
		sprintf(label,"goto __DO%x_BGN;",syntax.curly[pos].index);
		syntax.curly[syntax.curly_count++].type = TYPE_NULL;
		parse_line(label);
		syntax.curly_count--;

		// 条件終了地点のラベル形成する
		sprintf(label,"__DO%x_FIN",syntax.curly[pos].index);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);
		p = skip_space(p);
		if(*p != ';') {
			disp_error_message("need ';'",p);
			return p+1;
		}
		p++;
		syntax.curly_count--;
		return p;
	} else if(syntax.curly[pos].type == TYPE_FOR) {
		// 次のループに飛ばす
		sprintf(label,"goto __FR%x_NXT;",syntax.curly[pos].index);
		syntax.curly[syntax.curly_count++].type = TYPE_NULL;
		parse_line(label);
		syntax.curly_count--;

		// for 終了のラベル付け
		sprintf(label,"__FR%x_FIN",syntax.curly[pos].index);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);
		syntax.curly_count--;
		return p;
	} else if(syntax.curly[pos].type == TYPE_WHILE) {
		// while 条件判断へ飛ばす
		sprintf(label,"goto __WL%x_NXT;",syntax.curly[pos].index);
		syntax.curly[syntax.curly_count++].type = TYPE_NULL;
		parse_line(label);
		syntax.curly_count--;

		// while 終了のラベル付け
		sprintf(label,"__WL%x_FIN",syntax.curly[pos].index);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);
		syntax.curly_count--;
		return p;
	} else if(syntax.curly[syntax.curly_count-1].type == TYPE_USERFUNC) {
		int pos = syntax.curly_count-1;
		char label[256];
		int l;
		// 戻す
		sprintf(label,"return;");
		syntax.curly[syntax.curly_count++].type = TYPE_NULL;
		parse_line(label);
		syntax.curly_count--;

		// 現在地のラベルを付ける
		sprintf(label,"__FN%x_FIN",syntax.curly[pos].index);
		l=add_str(label);
		if(str_data[l].label!=-1){
			disp_error_message("dup label ",p);
			exit(1);
		}
		set_label(l,script_pos);
		syntax.curly_count--;
		return p + 1;
	} else {
		*flag = 0;
		return p;
	}
}

/*==========================================
 * 組み込み関数の追加
 *------------------------------------------
 */
static void add_buildin_func(void)
{
	int i,n;
	for(i=0;buildin_func[i].func;i++){
		n=add_str((unsigned char *) buildin_func[i].name);
		str_data[n].type=C_FUNC;
		str_data[n].val=i;
		str_data[n].func=buildin_func[i].func;
	}
}

/*==========================================
 * 定数データベースの読み込み
 *------------------------------------------
 */
static void read_constdb(void)
{
	FILE *fp;
	char line[1024],name[1024];
	int val,n,i,type;

	sprintf(line, "%s/const.txt", db_path);
	fp=fopen(line, "r");
	if(fp==NULL){
		ShowError("can't read %s\n", line);
		return ;
	}
	while(fgets(line,1020,fp)){
		if(line[0]=='/' && line[1]=='/')
			continue;
		type=0;
		if(sscanf(line,"%[A-Za-z0-9_],%d,%d",name,&val,&type)>=2 ||
		   sscanf(line,"%[A-Za-z0-9_] %d %d",name,&val,&type)>=2){
			for(i=0;name[i];i++)
				name[i]=tolower(name[i]);
			n=add_str((const unsigned char *) name);
			if(type==0)
				str_data[n].type=C_INT;
			else
				str_data[n].type=C_PARAM;
			str_data[n].val=val;
		}
	}
	fclose(fp);
}

/*==========================================
 * スクリプトの解析
 *------------------------------------------
 */
unsigned char* parse_script(unsigned char *src,int line)
{
	unsigned char *p, *tmpp;
	int i;
	static int first = 1;

	if (first) {
		add_buildin_func();
		read_constdb();
	}
	first = 0;

//////////////////////////////////////////////
// additional check on the input to filter empty scripts ("{}" and "{ }")
	p = src;
	p = skip_space(p);
	if (*p != '{') {
		disp_error_message("not found '{'", p);
		return NULL;
	}
	p++;
	p = skip_space(p);
	if (*p == '}') {
		// an empty function, just return
		return NULL;
	}
	script_buf = (unsigned char *) aCallocA(SCRIPT_BLOCK_SIZE, sizeof(unsigned char));
	script_pos = 0;
	script_size = SCRIPT_BLOCK_SIZE;
	str_data[LABEL_NEXTLINE].type = C_NOP;
	str_data[LABEL_NEXTLINE].backpatch = -1;
	str_data[LABEL_NEXTLINE].label = -1;
	for (i = LABEL_START; i < str_num; i++) {
		if (
			str_data[i].type == C_POS || str_data[i].type == C_NAME ||
			str_data[i].type == C_USERFUNC || str_data[i].type == C_USERFUNC_POS
		) {
			str_data[i].type = C_NOP;
			str_data[i].backpatch = -1;
			str_data[i].label = -1;
		}
	}

	//Labels must be reparsed for the script....
	scriptlabel_db->clear(scriptlabel_db, NULL);

	// for error message
	startptr = src;
	startline = line;

	while (p && *p && (*p != '}' || syntax.curly_count != 0)) {
		p = skip_space(p);
		// labelだけ特殊処理
		tmpp = skip_space(skip_word(p));
		if (*tmpp == ':' && !(!strncmp(p,"default",7) && !isalpha(*(p + 7)))) {
			int l, c;
			c = *skip_word(p);
			*skip_word(p) = 0;
			l = add_str(p);
			if (str_data[l].label != -1) {
				*skip_word(p) = c;
				disp_error_message("dup label ", p);
				exit(1);
			}
			set_label(l, script_pos);
			strdb_put(scriptlabel_db, p, (void*)script_pos);	// 外部用label db登録
			*skip_word(p) = c;
			p = tmpp + 1;
			continue;
		}

		// 他は全部一緒くた
		p = parse_line(p);
		p = skip_space(p);
		add_scriptc(C_EOL);

		set_label(LABEL_NEXTLINE, script_pos);
		str_data[LABEL_NEXTLINE].type = C_NOP;
		str_data[LABEL_NEXTLINE].backpatch = -1;
		str_data[LABEL_NEXTLINE].label = -1;
	}

	add_scriptc(C_NOP);

	script_size = script_pos;
	script_buf = (unsigned char *)aRealloc(script_buf, script_pos + 1);

	// 未解決のラベルを解決
	for (i = LABEL_START; i < str_num; i++) {
		if (str_data[i].type == C_NOP) {
			int j, next;
			str_data[i].type = C_NAME;
			str_data[i].label = i;
			for (j = str_data[i].backpatch; j >= 0 && j != 0x00ffffff; ) {
				next = (*(int*)(script_buf+j)) & 0x00ffffff;
				script_buf[j] = i;
				script_buf[j+1] = i>>8;
				script_buf[j+2] = i>>16;
				j = next;
			}
		}
	}

#ifdef DEBUG_DISP
	for (i = 0; i < script_pos; i++) {
		if ((i & 15) == 0) printf("%04x : ", i);
		printf("%02x ", script_buf[i]);
		if((i&15) == 15) printf("\n");
	}
	printf("\n");
#endif

	startptr = NULL; //Clear pointer to prevent future references to a src that may be free'd. [Skotlex]
	return (unsigned char *) script_buf;
}

//
// 実行系
//
enum {RUN = 0,STOP,END,RERUNLINE,GOTO,RETFUNC};

/*==========================================
 * ridからsdへの解決
 *------------------------------------------
 */
struct map_session_data *script_rid2sd(struct script_state *st)
{
	struct map_session_data *sd=map_id2sd(st->rid);
	if(!sd){
		ShowError("script_rid2sd: fatal error ! player not attached!\n");
		report_src(st);
	}
	return sd;
}


/*==========================================
 * 変数の読み取り
 *------------------------------------------
 */
int get_val(struct script_state*st,struct script_data* data)
{
	struct map_session_data *sd=NULL;
	if(data->type==C_NAME){
		char *name=str_buf+str_data[data->u.num&0x00ffffff].str;
		char prefix=*name;
		char postfix=name[strlen(name)-1];

		if(prefix!='$'){
			if((sd=script_rid2sd(st))==NULL)
				ShowError("get_val error name?:%s\n",name);
		}
		if(postfix=='$'){

			data->type=C_CONSTSTR;
			if( prefix=='@'){
				if(sd)
				data->u.str = pc_readregstr(sd,data->u.num);
			}else if(prefix=='$'){
				data->u.str = (char *)idb_get(mapregstr_db,data->u.num);
			}else if(prefix=='#'){
				if( name[1]=='#'){
					if(sd)
					data->u.str = pc_readaccountreg2str(sd,name);
				}else{
					if(sd)
					data->u.str = pc_readaccountregstr(sd,name);
				}
 			}else{
				if(sd)
				data->u.str = pc_readglobalreg_str(sd,name);
 			} // [zBuffer]
			/*else{
				ShowWarning("script: get_val: illegal scope string variable.\n");
				data->u.str = "!!ERROR!!";
			}*/
			if( data->u.str == NULL )
				data->u.str ="";

		}else{

			data->type=C_INT;
			if(str_data[data->u.num&0x00ffffff].type==C_INT){
				data->u.num = str_data[data->u.num&0x00ffffff].val;
			}else if(str_data[data->u.num&0x00ffffff].type==C_PARAM){
				if(sd)
				data->u.num = pc_readparam(sd,str_data[data->u.num&0x00ffffff].val);
			}else if(prefix=='@'){
				if(sd)
				data->u.num = pc_readreg(sd,data->u.num);
			}else if(prefix=='$'){
				data->u.num = (int)idb_get(mapreg_db,data->u.num);
			}else if(prefix=='#'){
				if( name[1]=='#'){
					if(sd)
					data->u.num = pc_readaccountreg2(sd,name);
				}else{
					if(sd)
					data->u.num = pc_readaccountreg(sd,name);
				}
			}else{
				if(sd)
				data->u.num = pc_readglobalreg(sd,name);
			}
		}
	}
	return 0;
}
/*==========================================
 * 変数の読み取り2
 *------------------------------------------
 */
void* get_val2(struct script_state*st,int num)
{
	struct script_data dat;
	dat.type=C_NAME;
	dat.u.num=num;
	get_val(st,&dat);
	if( dat.type==C_INT ) return (void*)dat.u.num;
	else return (void*)dat.u.str;
}

/*==========================================
 * 変数設定用
 *------------------------------------------
 */
static int set_reg(struct map_session_data *sd,int num,char *name,void *v)
{
	char prefix=*name;
	char postfix=name[strlen(name)-1];

	if( postfix=='$' ){
		char *str=(char*)v;
		if( prefix=='@'){
			pc_setregstr(sd,num,str);
		}else if(prefix=='$') {
			mapreg_setregstr(num,str);
		}else if(prefix=='#') {
			if( name[1]=='#' )
				pc_setaccountreg2str(sd,name,str);
			else
				pc_setaccountregstr(sd,name,str);
 		}else{
			pc_setglobalreg_str(sd,name,str);
 		} // [zBuffer]

		/*else{
			ShowWarning("script: set_reg: illegal scope string variable !");
		}*/
	}else{
		// 数値
		int val = (int)v;
		if(str_data[num&0x00ffffff].type==C_PARAM){
			pc_setparam(sd,str_data[num&0x00ffffff].val,val);
		}else if(prefix=='@') {
			pc_setreg(sd,num,val);
		}else if(prefix=='$') {
			mapreg_setreg(num,val);
		}else if(prefix=='#') {
			if( name[1]=='#' )
				pc_setaccountreg2(sd,name,val);
			else
				pc_setaccountreg(sd,name,val);
		}else{
			pc_setglobalreg(sd,name,val);
		}
	}
	return 0;
}

int set_var(struct map_session_data *sd, char *name, void *val)
{
    return set_reg(sd, add_str((unsigned char *) name), name, val);
}

/*==========================================
 * 文字列への変換
 *------------------------------------------
 */
char* conv_str(struct script_state *st,struct script_data *data)
{
	get_val(st,data);
	if(data->type==C_INT){
		char *buf;
		buf=(char *)aCallocA(ITEM_NAME_LENGTH,sizeof(char));
		snprintf(buf,ITEM_NAME_LENGTH, "%d",data->u.num);
		data->type=C_STR;
		data->u.str=buf;
#if 1
	} else if(data->type==C_NAME){
		// テンポラリ。本来無いはず
		data->type=C_CONSTSTR;
		data->u.str=str_buf+str_data[data->u.num].str;
#endif
	}
	return data->u.str;
}

/*==========================================
 * 数値へ変換
 *------------------------------------------
 */
int conv_num(struct script_state *st,struct script_data *data)
{
	char *p;
	get_val(st,data);
	if(data->type==C_STR || data->type==C_CONSTSTR){
		p=data->u.str;
		data->u.num = atoi(p);
		if(data->type==C_STR)
			aFree(p);
		data->type=C_INT;
	}
	return data->u.num;
}

/*==========================================
 * スタックへ数値をプッシュ
 *------------------------------------------
 */
void push_val(struct script_stack *stack,int type,int val)
{
	if(stack->sp >= stack->sp_max){
		stack->sp_max += 64;
		stack->stack_data = (struct script_data *)aRealloc(stack->stack_data,
			sizeof(stack->stack_data[0]) * stack->sp_max);
		memset(stack->stack_data + (stack->sp_max - 64), 0,
			64 * sizeof(*(stack->stack_data)));
	}
//	if(battle_config.etc_log)
//		printf("push (%d,%d)-> %d\n",type,val,stack->sp);
	stack->stack_data[stack->sp].type=type;
	stack->stack_data[stack->sp].u.num=val;
	stack->sp++;
}

/*==========================================
 * スタックへ文字列をプッシュ
 *------------------------------------------
 */
void push_str(struct script_stack *stack,int type,unsigned char *str)
{
	if(stack->sp>=stack->sp_max){
		stack->sp_max += 64;
		stack->stack_data = (struct script_data *)aRealloc(stack->stack_data,
			sizeof(stack->stack_data[0]) * stack->sp_max);
		memset(stack->stack_data + (stack->sp_max - 64), '\0',
			64 * sizeof(*(stack->stack_data)));
	}
//	if(battle_config.etc_log)
//		printf("push (%d,%x)-> %d\n",type,str,stack->sp);
	stack->stack_data[stack->sp].type=type;
	stack->stack_data[stack->sp].u.str=(char *) str;
	stack->sp++;
}

/*==========================================
 * スタックへ複製をプッシュ
 *------------------------------------------
 */
void push_copy(struct script_stack *stack,int pos)
{
	switch(stack->stack_data[pos].type){
	case C_CONSTSTR:
		push_str(stack,C_CONSTSTR,(unsigned char *) stack->stack_data[pos].u.str);
		break;
	case C_STR:
		push_str(stack,C_STR,(unsigned char *) aStrdup(stack->stack_data[pos].u.str));
		break;
	default:
		push_val(stack,stack->stack_data[pos].type,stack->stack_data[pos].u.num);
		break;
	}
}

/*==========================================
 * スタックからポップ
 *------------------------------------------
 */
void pop_stack(struct script_stack* stack,int start,int end)
{
	int i;
	for(i=start;i<end;i++){
		if(stack->stack_data[i].type==C_STR){
			aFree(stack->stack_data[i].u.str);
			stack->stack_data[i].type=C_INT;  //Might not be correct, but it's done in case to prevent pointer errors later on. [Skotlex]
		}
	}
	if(stack->sp>end){
		memmove(&stack->stack_data[start],&stack->stack_data[end],sizeof(stack->stack_data[0])*(stack->sp-end));
	}
	stack->sp-=end-start;
}

/*==========================================
 * Free's the whole stack. Invoked when clearing a character. [Skotlex]
 *------------------------------------------
 */
void script_free_stack(struct script_stack* stack)
{
	int i;
	for (i = 0; i < stack->sp; i++)
	{
		if(stack->stack_data[i].type==C_STR)
		{
			//ShowDebug ("script_free_stack: freeing %p at sp=%d.\n", stack->stack_data[i].u.str, i);
			aFree(stack->stack_data[i].u.str);
			stack->stack_data[i].type = C_INT;
		}
	}
	aFree (stack->stack_data);
	aFree (stack);
}

int axtoi(char *hexStg) {
	int n = 0;         // position in string
	int m = 0;         // position in digit[] to shift
	int count;         // loop index
	int intValue = 0;  // integer value of hex string
	int digit[11];      // hold values to convert
	while (n < 10) {
		if (hexStg[n]=='\0')
			break;
		if (hexStg[n] > 0x29 && hexStg[n] < 0x40 ) //if 0 to 9
			digit[n] = hexStg[n] & 0x0f;            //convert to int
		else if (hexStg[n] >='a' && hexStg[n] <= 'f') //if a to f
			digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
		else if (hexStg[n] >='A' && hexStg[n] <= 'F') //if A to F
			digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
		else break;
		n++;
	}
	count = n;
	m = n - 1;
	n = 0;
	while(n < count) {
		// digit[n] is value of hex digit at position n
		// (m << 2) is the number of positions to shift
		// OR the bits into return value
		intValue = intValue | (digit[n] << (m << 2));
		m--;   // adjust the position to set
		n++;   // next digit to process
	}
	return (intValue);
}

// [Lance] Hex string to integer converter
int buildin_axtoi(struct script_state *st)
{
	char *hex = conv_str(st,& (st->stack->stack_data[st->start+2]));
	push_val(st->stack, C_INT, axtoi(hex));
	return 0;
}

//
// 埋め込み関数
//
/*==========================================
 *
 *------------------------------------------
 */
int buildin_mes(struct script_state *st)
{
	conv_str(st,& (st->stack->stack_data[st->start+2]));
	clif_scriptmes(script_rid2sd(st),st->oid,st->stack->stack_data[st->start+2].u.str);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_goto(struct script_state *st)
{
	int pos;

	if (st->stack->stack_data[st->start+2].type != C_POS){
		int func = st->stack->stack_data[st->start+2].u.num;
		ShowMessage("script: goto '"CL_WHITE"%s"CL_RESET"': not label!\n", str_buf + str_data[func].str);
		st->state = END;
		return 1;
	}

	pos = conv_num(st,& (st->stack->stack_data[st->start+2]));
	st->pos = pos;
	st->state = GOTO;
	return 0;
}

/*==========================================
 * ユーザー定義関数の呼び出し
 *------------------------------------------
 */
int buildin_callfunc(struct script_state *st)
{
	char *scr;
	char *str=conv_str(st,& (st->stack->stack_data[st->start+2]));

	if( (scr=(char *) strdb_get(userfunc_db,(unsigned char*)str)) ){
		int i,j;
		for(i=st->start+3,j=0;i<st->end;i++,j++)
			push_copy(st->stack,i);

		push_val(st->stack,C_INT,j);				// 引数の数をプッシュ
		push_val(st->stack,C_INT,st->stack->defsp);	// 現在の基準スタックポインタをプッシュ
		push_val(st->stack,C_INT,(int)st->script);	// 現在のスクリプトをプッシュ
		push_val(st->stack,C_RETINFO,st->pos);		// 現在のスクリプト位置をプッシュ

		st->pos=0;
		st->script=scr;
		st->stack->defsp=st->start+4+j;
		st->state=GOTO;
	}else{
		ShowWarning("script:callfunc: function not found! [%s]\n",str);
		st->state=END;
		return 1;
	}
	return 0;
}
/*==========================================
 * サブルーティンの呼び出し
 *------------------------------------------
 */
int buildin_callsub(struct script_state *st)
{
	int pos=conv_num(st,& (st->stack->stack_data[st->start+2]));
	int i,j;
	if(st->stack->stack_data[st->start+2].type != C_POS && st->stack->stack_data[st->start+2].type != C_USERFUNC_POS) {
		ShowError("script: callsub: not label !\n");
		st->state=END;
		return 1;
	} else {
		for(i=st->start+3,j=0;i<st->end;i++,j++)
			push_copy(st->stack,i);

		push_val(st->stack,C_INT,j);				// 引数の数をプッシュ
		push_val(st->stack,C_INT,st->stack->defsp);	// 現在の基準スタックポインタをプッシュ
		push_val(st->stack,C_INT,(int)st->script);	// 現在のスクリプトをプッシュ
		push_val(st->stack,C_RETINFO,st->pos);		// 現在のスクリプト位置をプッシュ

		st->pos=pos;
		st->stack->defsp=st->start+4+j;
		st->state=GOTO;
	}
	return 0;
}

/*==========================================
 * 引数の所得
 *------------------------------------------
 */
int buildin_getarg(struct script_state *st)
{
	int num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	int max,stsp;
	if( st->stack->defsp<4 || st->stack->stack_data[st->stack->defsp-1].type!=C_RETINFO ){
		ShowWarning("script:getarg without callfunc or callsub!\n");
		st->state=END;
		return 1;
	}
	max=conv_num(st,& (st->stack->stack_data[st->stack->defsp-4]));
	stsp=st->stack->defsp - max -4;
	if( num >= max ){
		ShowWarning("script:getarg arg1(%d) out of range(%d) !\n",num,max);
		st->state=END;
		return 1;
	}
	push_copy(st->stack,stsp+num);
	return 0;
}

/*==========================================
 * サブルーチン/ユーザー定義関数の終了
 *------------------------------------------
 */
int buildin_return(struct script_state *st)
{
	if(st->end>st->start+2){	// 戻り値有り
		push_copy(st->stack,st->start+2);
	}
	st->state=RETFUNC;
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_next(struct script_state *st)
{
	st->state=STOP;
	clif_scriptnext(script_rid2sd(st),st->oid);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_close(struct script_state *st)
{
	st->state=END;
	clif_scriptclose(script_rid2sd(st),st->oid);
	return 0;
}
int buildin_close2(struct script_state *st)
{
	st->state=STOP;
	clif_scriptclose(script_rid2sd(st),st->oid);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_menu(struct script_state *st)
{
	char *buf;
	int len,i;
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	if(sd->state.menu_or_input==0){
		st->state=RERUNLINE;
		sd->state.menu_or_input=1;
		for(i=st->start+2,len=16;i<st->end;i+=2){
			conv_str(st,& (st->stack->stack_data[i]));
			len+=(int)strlen(st->stack->stack_data[i].u.str)+1;
		}
		buf=(char *)aCallocA(len+1,sizeof(char));
		buf[0]=0;
		for(i=st->start+2,len=0;i<st->end;i+=2){
			strcat(buf,st->stack->stack_data[i].u.str);
			strcat(buf,":");
		}
		clif_scriptmenu(script_rid2sd(st),st->oid,buf);
		aFree(buf);
	} else if(sd->npc_menu==0xff){	// cansel
		sd->state.menu_or_input=0;
		st->state=END;
	} else {	// goto動作
		sd->state.menu_or_input=0;
		if(sd->npc_menu>0){
			//Skip empty menu entries which weren't displayed on the client (blackhole89)
			for(i=st->start+2;i<=(st->start+sd->npc_menu*2) && sd->npc_menu<(st->end-st->start)/2;i+=2)
			{
				if((int)strlen(st->stack->stack_data[i].u.str) < 1)
					sd->npc_menu++; //Empty selection which wasn't displayed on the client.
			}
			if(sd->npc_menu >= (st->end-st->start)/2) {
				//Invalid selection.
				st->state=END;
				return 0;
			}
			if( st->stack->stack_data[st->start+sd->npc_menu*2+1].type!=C_POS ){
				ShowError("script: menu: not label !\n");
				st->state=END;
				return 1;
			}
			pc_setreg(sd,add_str((unsigned char *) "@menu"),sd->npc_menu);
			st->pos= conv_num(st,& (st->stack->stack_data[st->start+sd->npc_menu*2+1]));
			st->state=GOTO;
		}
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_menu2(struct script_state *st)
{
	char *buf;
	int len,i;
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	if(sd->state.menu_or_input==0){
		st->state=RERUNLINE;
		sd->state.menu_or_input=1;
		for(i=st->start+2,len=16;i<st->end;i+=2){
			conv_str(st,& (st->stack->stack_data[i]));
			len+=(int)strlen(st->stack->stack_data[i].u.str)+1;
		}
		buf=(char *)aCallocA(len+1,sizeof(char));
		buf[0]=0;
		for(i=st->start+2,len=0;i<st->end;i+=2){
			strcat(buf,st->stack->stack_data[i].u.str);
			strcat(buf,":");
		}
		clif_scriptmenu(script_rid2sd(st),st->oid,buf);
		aFree(buf);
	} else {
		pc_setreg(sd,add_str((unsigned char *) "@menu"),sd->npc_menu);
		sd->state.menu_or_input=0;
		if(sd->npc_menu>0 && sd->npc_menu<(st->end-st->start)/2){
			int pos;
			if( st->stack->stack_data[st->start+sd->npc_menu*2+1].type!=C_POS ){
				ShowError("script: menu: not label !\n");
				st->state=END;
				return 0;
			}
			pos=conv_num(st,& (st->stack->stack_data[st->start+sd->npc_menu*2+1]));
			st->pos=pos;
			st->state=GOTO;
		}
	}
	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_rand(struct script_state *st)
{
	int range;

	if (st->end > st->start+3){
		int min, max;
		min = conv_num(st,& (st->stack->stack_data[st->start+2]));
		max = conv_num(st,& (st->stack->stack_data[st->start+3]));
		if (max == min){ //Why would someone do this?
			push_val(st->stack,C_INT,min);
			return 0;
		}
		if (max < min){
			int tmp = min;
			min = max;
			max = tmp;
		}
		range = max - min + 1;
		if (range == 0) range = 1;
		push_val(st->stack,C_INT,rand()%range+min);
	} else {
		range = conv_num(st,& (st->stack->stack_data[st->start+2]));
		if (range == 0) range = 1;
		push_val(st->stack,C_INT,rand()%range);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_warp(struct script_state *st)
{
	int x,y;
	char *str;
	struct map_session_data *sd=script_rid2sd(st);

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));
	if(strcmp(str,"Random")==0)
		pc_randomwarp(sd,3);
	else if(strcmp(str,"SavePoint")==0){
		if(map[sd->bl.m].flag.noreturn)	// 蝶禁止
			return 0;

		pc_setpos(sd,sd->status.save_point.map,
			sd->status.save_point.x,sd->status.save_point.y,3);
	}else if(strcmp(str,"Save")==0){
		if(map[sd->bl.m].flag.noreturn)	// 蝶禁止
			return 0;

		pc_setpos(sd,sd->status.save_point.map,
			sd->status.save_point.x,sd->status.save_point.y,3);
	}else
		pc_setpos(sd,mapindex_name2id(str),x,y,0);
	return 0;
}
/*==========================================
 * エリア指定ワープ
 *------------------------------------------
 */
int buildin_areawarp_sub(struct block_list *bl,va_list ap)
{
	int x,y;
	unsigned int map;
	map=va_arg(ap, unsigned int);
	x=va_arg(ap,int);
	y=va_arg(ap,int);
	if(map == 0)
		pc_randomwarp((struct map_session_data *)bl,3);
	else
		pc_setpos((struct map_session_data *)bl,map,x,y,0);
	return 0;
}
int buildin_areawarp(struct script_state *st)
{
	int x,y,m;
	unsigned int index;
	char *str;
	char *mapname;
	int x0,y0,x1,y1;

	mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x0=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y0=conv_num(st,& (st->stack->stack_data[st->start+4]));
	x1=conv_num(st,& (st->stack->stack_data[st->start+5]));
	y1=conv_num(st,& (st->stack->stack_data[st->start+6]));
	str=conv_str(st,& (st->stack->stack_data[st->start+7]));
	x=conv_num(st,& (st->stack->stack_data[st->start+8]));
	y=conv_num(st,& (st->stack->stack_data[st->start+9]));

	if( (m=map_mapname2mapid(mapname))< 0)
		return 0;

	if(strcmp(str,"Random")==0)
		index = 0;
	else if(!(index=mapindex_name2id(str)))
		return 0;

	map_foreachinarea(buildin_areawarp_sub,
		m,x0,y0,x1,y1,BL_PC,	index,x,y );
	return 0;
}

/*==========================================
 * warpchar [LuzZza]
 * Useful for warp one player from
 * another player npc-session.
 * Using: warpchar "mapname.gat",x,y,Char_ID;
 *------------------------------------------
 */
int buildin_warpchar(struct script_state *st)
{
	int x,y,a,i;
	char *str;
	struct map_session_data *sd, **pl_allsd;
	int users;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));
	a=conv_num(st,& (st->stack->stack_data[st->start+5]));

	pl_allsd = map_getallusers(&users);

	for(i=0; i<users; i++) {
		sd = pl_allsd[i];
		if(sd->status.char_id == a) {

			if(strcmp(str, "Random") == 0)
				pc_randomwarp(sd, 3);

			else if(strcmp(str, "SavePoint") == 0)
				pc_setpos(sd, sd->status.save_point.map,
					sd->status.save_point.x, sd->status.save_point.y, 3);

			else
				pc_setpos(sd, mapindex_name2id(str), x, y, 3);
		}
	}

	return 0;
}

/*==========================================
 * Warpparty - [Fredzilla]
 * Syntax: warpparty "mapname.gat",x,y,Party_ID;
 *------------------------------------------
 */
int buildin_warpparty(struct script_state *st)
{
	int x,y;
	char *str;
	int p_id;
	int i;
	unsigned short mapindex;
	struct map_session_data *pl_sd;
	struct party *p=NULL;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));
	p_id=conv_num(st,& (st->stack->stack_data[st->start+5]));
	if(p_id < 1)
		return 0;
	p = party_search(p_id);
	if (!p)
		return 0;
	if(strcmp(str,"Random")==0)
	{
		for (i = 0; i < MAX_PARTY; i++)
		{
			if ((pl_sd = p->member[i].sd))
			{
				if(map[pl_sd->bl.m].flag.nowarp)
					continue;
				pc_randomwarp(pl_sd,3);
			}
		}
	}
	else if(strcmp(str,"SavePointAll")==0)
	{
		for (i = 0; i < MAX_PARTY; i++)
		{
			if ((pl_sd = p->member[i].sd))
			{
				if(map[pl_sd->bl.m].flag.noreturn)
					continue;
				pc_setpos(pl_sd,pl_sd->status.save_point.map,pl_sd->status.save_point.x,pl_sd->status.save_point.y,3);
			}
		}
	}
	else if(strcmp(str,"SavePoint")==0)
	{
		pl_sd=script_rid2sd(st);
		if (!pl_sd) return 0;

		mapindex=pl_sd->status.save_point.map;
		x=pl_sd->status.save_point.x;
		y=pl_sd->status.save_point.y;

		for (i = 0; i < MAX_PARTY; i++)
		{
			if ((pl_sd = p->member[i].sd))
			{
				if(map[pl_sd->bl.m].flag.noreturn)
					continue;
				pc_setpos(pl_sd,mapindex,x,y,3);
			}
		}
	}
	else
	{
		mapindex = mapindex_name2id(str);
		if (!mapindex) //Show source of npc error.
			return 1;
		for (i = 0; i < MAX_PARTY; i++)
		{
			if ((pl_sd = p->member[i].sd))
			{
				if(map[pl_sd->bl.m].flag.noreturn || map[pl_sd->bl.m].flag.nowarp)
					continue;
				pc_setpos(pl_sd,mapindex,x,y,3);
			}
		}
	}
	return 0;
}
/*==========================================
 * Warpguild - [Fredzilla]
 * Syntax: warpguild "mapname.gat",x,y,Guild_ID;
 *------------------------------------------
 */
int buildin_warpguild(struct script_state *st)
{
	int x,y;
	unsigned short mapindex;
	char *str;
	int g;
	int i;
	struct map_session_data *pl_sd, **pl_allsd;
	int users;
	struct map_session_data *sd;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));
	g=conv_num(st,& (st->stack->stack_data[st->start+5]));
	sd=script_rid2sd(st);

	if(map[sd->bl.m].flag.noreturn || map[sd->bl.m].flag.nowarpto)
		return 0;

	if(g < 1)
		return 0;

	pl_allsd = map_getallusers(&users);

	if(strcmp(str,"Random")==0)
	{

		for (i = 0; i < users; i++)
		{
			if ((pl_sd = pl_allsd[i]) && pl_sd->status.guild_id == g)
			{
				if(map[pl_sd->bl.m].flag.nowarp)
					continue;
				pc_randomwarp(pl_sd,3);
			}
		}
	}
	else if(strcmp(str,"SavePointAll")==0)
	{
		if(map[sd->bl.m].flag.noreturn)
			return 0;

		for (i = 0; i < users; i++)
		{
			if ((pl_sd = pl_allsd[i]) && pl_sd->status.guild_id == g)
			{
				if(map[pl_sd->bl.m].flag.noreturn)
					continue;
				pc_setpos(pl_sd,pl_sd->status.save_point.map,pl_sd->status.save_point.x,pl_sd->status.save_point.y,3);
			}
		}
	}
	else if(strcmp(str,"SavePoint")==0)
	{
		if(map[sd->bl.m].flag.noreturn)
			return 0;

		mapindex=sd->status.save_point.map;
		x=sd->status.save_point.x;
		y=sd->status.save_point.y;
		for (i = 0; i < users; i++)
		{
			if ((pl_sd = pl_allsd[i]) && pl_sd->status.guild_id == g)
			{
				if(map[pl_sd->bl.m].flag.noreturn)
					continue;
				pc_setpos(pl_sd,mapindex,x,y,3);
			}
		}
	}
	else
	{
		mapindex = mapindex_name2id(str);
		for (i = 0; i < users; i++)
		{
			if ((pl_sd = pl_allsd[i]) && pl_sd->status.guild_id == g)
			{
				if(map[pl_sd->bl.m].flag.noreturn || map[pl_sd->bl.m].flag.nowarp)
					continue;
				pc_setpos(pl_sd,mapindex,x,y,3);
			}
		}
	}
	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_heal(struct script_state *st)
{
	int hp,sp;

	hp=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sp=conv_num(st,& (st->stack->stack_data[st->start+3]));
	pc_heal(script_rid2sd(st),hp,sp);
	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_itemheal(struct script_state *st)
{
	int hp,sp;

	hp=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sp=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if(potion_flag==1) {
		potion_hp = hp;
		potion_sp = sp;
		return 0;
	}

	pc_itemheal(script_rid2sd(st),hp,sp);
	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_percentheal(struct script_state *st)
{
	int hp,sp;

	hp=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sp=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if(potion_flag==1) {
		potion_per_hp = hp;
		potion_per_sp = sp;
		return 0;
	}

	pc_percentheal(script_rid2sd(st),hp,sp);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_jobchange(struct script_state *st)
{
	int job, upper=-1;

	job=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if( st->end>st->start+3 )
		upper=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if ((job >= 0 && job < MAX_PC_CLASS))
		pc_jobchange(script_rid2sd(st),job, upper);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_input(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	int num=(st->end>st->start+2)?st->stack->stack_data[st->start+2].u.num:0;
	char *name=(char *) ((st->end>st->start+2)?str_buf+str_data[num&0x00ffffff].str:"");
//	char prefix=*name;
	char postfix=name[strlen(name)-1];

	sd=script_rid2sd(st);
	if(sd->state.menu_or_input){
		sd->state.menu_or_input=0;
		if( postfix=='$' ){
			// 文字列
			if(st->end>st->start+2){ // 引数1個
				set_reg(sd,num,name,(void*)sd->npc_str);
			}else{
				ShowError("buildin_input: string discarded !!\n");
				return 1;
			}
			return 0;
		}
		// commented by Lupus (check Value Number Input fix in clif.c)
		// readded by Yor: set ammount to 0 instead of cancel trade.
		// ** Fix by fritz :X keeps people from abusing old input bugs
		if (sd->npc_amount < 0) { //** If input amount is less then 0
//				clif_tradecancelled(sd); // added "Deal has been cancelled" message by Valaris
//				buildin_close(st); // ** close
			sd->npc_amount = 0;
		} else if ((unsigned int)sd->npc_amount > battle_config.vending_max_value) // new fix by Yor
			sd->npc_amount = battle_config.vending_max_value;

		// 数値
		if(st->end>st->start+2){ // 引数1個
			set_reg(sd,num,name,(void*)sd->npc_amount);
		} else {
			// ragemu互換のため
			pc_setreg(sd,add_str((unsigned char *) "l14"),sd->npc_amount);
		}
		return 0;
	}
	//state.menu_or_input = 0
	st->state=RERUNLINE;
	if(postfix=='$')
		clif_scriptinputstr(sd,st->oid);
	else
		clif_scriptinput(sd,st->oid);
	sd->state.menu_or_input=1;
	return 0;
}

/*==========================================
 * 変数設定
 *------------------------------------------
 */
int buildin_set(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	int num=st->stack->stack_data[st->start+2].u.num;
	char *name=str_buf+str_data[num&0x00ffffff].str;
	char prefix=*name;
	char postfix=name[strlen(name)-1];

	if( st->stack->stack_data[st->start+2].type!=C_NAME ){
		ShowError("script: buildin_set: not name\n");
		return 1;
	}

	if( prefix!='$' )
		sd=script_rid2sd(st);


	if( postfix=='$' ){
		// 文字列
		char *str = conv_str(st,& (st->stack->stack_data[st->start+3]));
		set_reg(sd,num,name,(void*)str);
	}else{
		// 数値
		int val = conv_num(st,& (st->stack->stack_data[st->start+3]));
		set_reg(sd,num,name,(void*)val);
	}

	return 0;
}
/*==========================================
 * 配列変数設定
 *------------------------------------------
 */
int buildin_setarray(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	int num=st->stack->stack_data[st->start+2].u.num;
	char *name=str_buf+str_data[num&0x00ffffff].str;
	char prefix=*name;
	char postfix=name[strlen(name)-1];
	int i,j;

	if( prefix!='$' && prefix!='@' ){
		ShowWarning("buildin_setarray: illegal scope !\n");
		return 1;
	}
	if( prefix!='$' )
		sd=script_rid2sd(st);

	for(j=0,i=st->start+3; i<st->end && j<128;i++,j++){
		void *v;
		if( postfix=='$' )
			v=(void*)conv_str(st,& (st->stack->stack_data[i]));
		else
			v=(void*)conv_num(st,& (st->stack->stack_data[i]));
		set_reg( sd, num+(j<<24), name, v);
	}
	return 0;
}
/*==========================================
 * 配列変数クリア
 *------------------------------------------
 */
int buildin_cleararray(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	int num=st->stack->stack_data[st->start+2].u.num;
	char *name=str_buf+str_data[num&0x00ffffff].str;
	char prefix=*name;
	char postfix=name[strlen(name)-1];
	int sz=conv_num(st,& (st->stack->stack_data[st->start+4]));
	int i;
	void *v;

	if( prefix!='$' && prefix!='@' ){
		ShowWarning("buildin_cleararray: illegal scope !\n");
		return 1;
	}
	if( prefix!='$' )
		sd=script_rid2sd(st);

	if( postfix=='$' )
		v=(void*)conv_str(st,& (st->stack->stack_data[st->start+3]));
	else
		v=(void*)conv_num(st,& (st->stack->stack_data[st->start+3]));

	for(i=0;i<sz;i++)
		set_reg(sd,num+(i<<24),name,v);
	return 0;
}
/*==========================================
 * 配列変数コピー
 *------------------------------------------
 */
int buildin_copyarray(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	int num=st->stack->stack_data[st->start+2].u.num;
	char *name=str_buf+str_data[num&0x00ffffff].str;
	char prefix=*name;
	char postfix=name[strlen(name)-1];
	int num2=st->stack->stack_data[st->start+3].u.num;
	char *name2=str_buf+str_data[num2&0x00ffffff].str;
	char prefix2=*name2;
	char postfix2=name2[strlen(name2)-1];
	int sz=conv_num(st,& (st->stack->stack_data[st->start+4]));
	int i;

	if( prefix!='$' && prefix!='@' && prefix2!='$' && prefix2!='@' ){
		ShowWarning("buildin_copyarray: illegal scope !\n");
		return 1;
	}
	if( (postfix=='$' || postfix2=='$') && postfix!=postfix2 ){
		ShowError("buildin_copyarray: type mismatch !\n");
		return 1;
	}
	if( prefix!='$' || prefix2!='$' )
		sd=script_rid2sd(st);

	// if two array is the same and (num > num2), bottom-up copy is required [Eoe / jA 1116]
	if((num & 0x00FFFFFF) == (num2 & 0x00FFFFFF) && (num & 0xFF000000) > (num2 & 0xFF000000)) {
		for(i=sz-1;i>=0;i--)
			set_reg(sd,num+(i<<24),name, get_val2(st,num2+(i<<24)) );
	} else {
		for(i=0;i<sz;i++)
			set_reg(sd,num+(i<<24),name, get_val2(st,num2+(i<<24)) );
	}

	return 0;
}
/*==========================================
 * 配列変数のサイズ所得
 *------------------------------------------
 */
static int getarraysize(struct script_state *st,int num,int postfix)
{
	int i=(num>>24),c=-1; // Moded to -1 because even if the first element is 0, it will still report as 1 [Lance]
	for(;i<128;i++){
		// num must be the first elements of array [Eoe / jA 1127]
		void *v=get_val2(st,(num & 0x00FFFFFF)+(i<<24));
		if(postfix=='$' && *((char*)v) ) c=i;
		if(postfix!='$' && (int)v )c=i;
	}
	return c+1;
}
int buildin_getarraysize(struct script_state *st)
{
	int num=st->stack->stack_data[st->start+2].u.num;
	char *name=str_buf+str_data[num&0x00ffffff].str;
	char prefix=*name;
	char postfix=name[strlen(name)-1];

	if( prefix!='$' && prefix!='@' ){
		ShowWarning("buildin_copyarray: illegal scope !\n");
		return 1;
	}

	push_val(st->stack,C_INT,getarraysize(st,num,postfix) );
	return 0;
}
/*==========================================
 * 配列変数から要素削除
 *------------------------------------------
 */
int buildin_deletearray(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	int num=st->stack->stack_data[st->start+2].u.num;
	char *name=str_buf+str_data[num&0x00ffffff].str;
	char prefix=*name;
	char postfix=name[strlen(name)-1];
	int count=1;
	int i,sz=getarraysize(st,num,postfix)-(num>>24)-count+1;


	if( (st->end > st->start+3) )
		count=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if( prefix!='$' && prefix!='@' ){
		ShowWarning("buildin_deletearray: illegal scope !\n");
		return 1;
	}
	if( prefix!='$' )
		sd=script_rid2sd(st);

	for(i=0;i<sz;i++){
		set_reg(sd,num+(i<<24),name, get_val2(st,num+((i+count)<<24) ) );
	}
	for(;i<(128-(num>>24));i++){
		if( postfix!='$' ) set_reg(sd,num+(i<<24),name, 0);
		if( postfix=='$' ) set_reg(sd,num+(i<<24),name, (void *) "");
	}
	return 0;
}

/*==========================================
 * 指定要素を表す値(キー)を所得する
 *------------------------------------------
 */
int buildin_getelementofarray(struct script_state *st)
{
	if( st->stack->stack_data[st->start+2].type==C_NAME ){
		int i=conv_num(st,& (st->stack->stack_data[st->start+3]));
		if(i>127 || i<0){
			ShowWarning("script: getelementofarray (operator[]): param2 illegal number %d\n",i);
			push_val(st->stack,C_INT,0);
			return 1;
		}else{
			push_val(st->stack,C_NAME,
				(i<<24) | st->stack->stack_data[st->start+2].u.num );
		}
	}else{
		ShowError("script: getelementofarray (operator[]): param1 not name !\n");
		push_val(st->stack,C_INT,0);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_setlook(struct script_state *st)
{
	int type,val;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	val=conv_num(st,& (st->stack->stack_data[st->start+3]));

	pc_changelook(script_rid2sd(st),type,val);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_cutin(struct script_state *st)
{
	int type;

	conv_str(st,& (st->stack->stack_data[st->start+2]));
	type=conv_num(st,& (st->stack->stack_data[st->start+3]));

	clif_cutin(script_rid2sd(st),st->stack->stack_data[st->start+2].u.str,type);

	return 0;
}
/*==========================================
 * カードのイラストを表示する
 *------------------------------------------
 */
int buildin_cutincard(struct script_state *st)
{
	int itemid;
	struct item_data *i_data;

	itemid=conv_num(st,& (st->stack->stack_data[st->start+2]));

	i_data = itemdb_exists(itemid);
	if (i_data)
		clif_cutin(script_rid2sd(st),i_data->cardillustname,4);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_viewpoint(struct script_state *st)
{
	int type,x,y,id,color;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));
	id=conv_num(st,& (st->stack->stack_data[st->start+5]));
	color=conv_num(st,& (st->stack->stack_data[st->start+6]));

	clif_viewpoint(script_rid2sd(st),st->oid,type,x,y,id,color);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_countitem(struct script_state *st)
{
	int nameid=0,count=0,i;
	struct map_session_data *sd;

	struct script_data *data;

	sd = script_rid2sd(st);

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data;
		if( (item_data = itemdb_searchname(name)) != NULL)
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	if (nameid>=500) //if no such ID then skip this iteration
		for(i=0;i<MAX_INVENTORY;i++){
			if(sd->status.inventory[i].nameid==nameid)
				count+=sd->status.inventory[i].amount;
		}
	else{
		if(battle_config.error_log)
			ShowError("wrong item ID : countitem(%i)\n",nameid);
		push_val(st->stack,C_INT,0);
		return 1;
	}
	push_val(st->stack,C_INT,count);
	return 0;
}

/*==========================================
 * countitem2(nameID,Identified,Refine,Attribute,Card0,Card1,Card2,Card3)	[Lupus]
 *	returns number of items that met the conditions
 *------------------------------------------
 */
int buildin_countitem2(struct script_state *st)
{
	int nameid=0,count=0,i;
	int iden,ref,attr,c1,c2,c3,c4;
	struct map_session_data *sd;

	struct script_data *data;

	sd = script_rid2sd(st);

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data;
		if( (item_data = itemdb_searchname(name)) != NULL)
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	iden=conv_num(st,& (st->stack->stack_data[st->start+3]));
	ref=conv_num(st,& (st->stack->stack_data[st->start+4]));
	attr=conv_num(st,& (st->stack->stack_data[st->start+5]));
	c1=conv_num(st,& (st->stack->stack_data[st->start+6]));
	c2=conv_num(st,& (st->stack->stack_data[st->start+7]));
	c3=conv_num(st,& (st->stack->stack_data[st->start+8]));
	c4=conv_num(st,& (st->stack->stack_data[st->start+9]));

	if (nameid>=500) //if no such ID then skip this iteration
		for(i=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid<=0 || sd->inventory_data[i] == NULL ||
			sd->status.inventory[i].amount<=0 || sd->status.inventory[i].nameid!=nameid ||
			sd->status.inventory[i].identify!=iden || sd->status.inventory[i].refine!=ref ||
			sd->status.inventory[i].attribute!=attr || sd->status.inventory[i].card[0]!=c1 ||
			sd->status.inventory[i].card[1]!=c2 || sd->status.inventory[i].card[2]!=c3 ||
			sd->status.inventory[i].card[3]!=c4)
			continue;

			count+=sd->status.inventory[i].amount;
		}
	else{
		if(battle_config.error_log)
			ShowError("wrong item ID : countitem2(%i)\n",nameid);
		push_val(st->stack,C_INT,0);
		return 1;
	}
	push_val(st->stack,C_INT,count);

	return 0;
}

/*==========================================
 * 重量チェック
 *------------------------------------------
 */
int buildin_checkweight(struct script_state *st)
{
	int nameid=0,amount,i;
	unsigned long weight;
	struct map_session_data *sd;
	struct script_data *data;

	sd = script_rid2sd(st);

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		if( item_data )
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	amount=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if ( amount<=0 || nameid<500 ) { //if get wrong item ID or amount<=0, don't count weight of non existing items
		push_val(st->stack,C_INT,0);
		ShowError("buildin_checkweight: Wrong item ID or amount.\n");
		return 1;
	}

	weight = itemdb_weight(nameid)*amount;
	if(amount > MAX_AMOUNT || weight + sd->weight > sd->max_weight){
		push_val(st->stack,C_INT,0);
	} else {
		//Check if the inventory ain't full.
		//TODO: Currently does not checks if you can just stack it on top of another item you already have....

		i = pc_search_inventory(sd,0);
		if (i >= 0) //Empty slot available.
			push_val(st->stack,C_INT,1);
		else //Inventory full
			push_val(st->stack,C_INT,0);

	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_getitem(struct script_state *st)
{
	int nameid,nameidsrc,amount,flag = 0;
	struct item item_tmp;
	struct map_session_data *sd;
	struct script_data *data;

	sd = script_rid2sd(st);

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		nameid=512; //Apple item ID
		if( item_data != NULL)
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	if ( ( amount=conv_num(st,& (st->stack->stack_data[st->start+3])) ) <= 0) {
		return 0; //return if amount <=0, skip the useles iteration
	}
	//Violet Box, Blue Box, etc - random item pick
	if((nameidsrc = nameid)<0) { // Save real ID of the source Box [Lupus]
		nameid=itemdb_searchrandomid(-nameid);

		if(log_config.present > 0)
			log_present(sd, -nameidsrc, nameid); //fixed missing ID by Lupus

		flag = 1;
	}

	if(nameid > 0) {
		memset(&item_tmp,0,sizeof(item_tmp));
		item_tmp.nameid=nameid;
		if(!flag)
			item_tmp.identify=1;
		else
			item_tmp.identify=!itemdb_isequip3(nameid);
		if( st->end>st->start+5 ) //アイテムを指定したIDに渡す
			sd=map_id2sd(conv_num(st,& (st->stack->stack_data[st->start+5])));
		if(sd == NULL) //アイテムを渡す相手がいなかったらお帰り
			return 0;
		if((flag = pc_additem(sd,&item_tmp,amount))) {
			clif_additem(sd,0,0,flag);
			if(pc_candrop(sd,nameid))
				map_addflooritem(&item_tmp,amount,sd->bl.m,sd->bl.x,sd->bl.y,NULL,NULL,NULL,0);
		}

		//Logs items, got from (N)PC scripts [Lupus]
		if(log_config.pick > 0 ) {
			log_pick(sd, "N", 0, nameid, amount, NULL);
		}
		//Logs

	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_getitem2(struct script_state *st)
{
	int nameid,amount,flag = 0;
	int iden,ref,attr,c1,c2,c3,c4;
	struct item_data *item_data;
	struct item item_tmp;
	struct map_session_data *sd;
	struct script_data *data;

	sd = script_rid2sd(st);

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		nameid=512; //Apple item ID
		if( item_data )
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	amount=conv_num(st,& (st->stack->stack_data[st->start+3]));
	iden=conv_num(st,& (st->stack->stack_data[st->start+4]));
	ref=conv_num(st,& (st->stack->stack_data[st->start+5]));
	attr=conv_num(st,& (st->stack->stack_data[st->start+6]));
	c1=conv_num(st,& (st->stack->stack_data[st->start+7]));
	c2=conv_num(st,& (st->stack->stack_data[st->start+8]));
	c3=conv_num(st,& (st->stack->stack_data[st->start+9]));
	c4=conv_num(st,& (st->stack->stack_data[st->start+10]));
	if( st->end>st->start+11 ) //アイテムを指定したIDに渡す
		sd=map_id2sd(conv_num(st,& (st->stack->stack_data[st->start+11])));
	if(sd == NULL) //アイテムを渡す相手がいなかったらお帰り
		return 0;

	if(nameid<0) { // ランダム
		nameid=itemdb_searchrandomid(-nameid);
		flag = 1;
	}

	if(nameid > 0) {
		memset(&item_tmp,0,sizeof(item_tmp));
		item_data=itemdb_exists(nameid);
		if (item_data == NULL)
			return -1;
		if(item_data->type==4 || item_data->type==5){
			if(ref > 10) ref = 10;
		}
		else if(item_data->type==7) {
			iden = 1;
			ref = 0;
		}
		else {
			iden = 1;
			ref = attr = 0;
		}

		item_tmp.nameid=nameid;
		if(!flag)
			item_tmp.identify=iden;
		else if(item_data->type==4 || item_data->type==5)
			item_tmp.identify=0;
		item_tmp.refine=ref;
		item_tmp.attribute=attr;
		item_tmp.card[0]=c1;
		item_tmp.card[1]=c2;
		item_tmp.card[2]=c3;
		item_tmp.card[3]=c4;
		if((flag = pc_additem(sd,&item_tmp,amount))) {
			clif_additem(sd,0,0,flag);
			map_addflooritem(&item_tmp,amount,sd->bl.m,sd->bl.x,sd->bl.y,NULL,NULL,NULL,0);
		}

		//Logs items, got from (N)PC scripts [Lupus]
		if(log_config.pick > 0 ) {
			log_pick(sd, "N", 0, nameid, amount, &item_tmp);
		}
		//Logs
	}

	return 0;
}

/*==========================================
 * gets an item with someone's name inscribed [Skotlex]
 * getinscribeditem item_num, character_name
 * Returned Qty is always 1, only works on equip-able
 * equipment
 *------------------------------------------
 */
int buildin_getnameditem(struct script_state *st)
{
	int nameid;
	struct item item_tmp;
	struct map_session_data *sd, *tsd;
	struct script_data *data;

	sd = script_rid2sd(st);
	if (sd == NULL)
	{	//Player not attached!
		push_val(st->stack,C_INT,0);
		return 0;
	}

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		if( item_data == NULL)
		{	//Failed
			push_val(st->stack,C_INT,0);
			return 0;
		}
		nameid = item_data->nameid;
	}else
		nameid = conv_num(st,data);

	if(!itemdb_exists(nameid) || !itemdb_isequip3(nameid))
	{	//We don't allow non-equipable/stackable items to be named
		//to avoid any qty exploits that could happen because of it.
		push_val(st->stack,C_INT,0);
		return 0;
	}

	data=&(st->stack->stack_data[st->start+3]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR )	//Char Name
		tsd=map_nick2sd(conv_str(st,data));
	else	//Char Id was given
		tsd=map_charid2sd(conv_num(st,data));

	if( tsd == NULL )
	{	//Failed
		push_val(st->stack,C_INT,0);
		return 0;
	}

	memset(&item_tmp,0,sizeof(item_tmp));
	item_tmp.nameid=nameid;
	item_tmp.amount=1;
	item_tmp.identify=1;
	item_tmp.card[0]=254; //we don't use 255! because for example SIGNED WEAPON shouldn't get TOP10 BS Fame bonus [Lupus]
	item_tmp.card[2]=tsd->status.char_id;
	item_tmp.card[3]=tsd->status.char_id >> 16;
	if(pc_additem(sd,&item_tmp,1)) {
		push_val(st->stack,C_INT,0);
		return 0;	//Failed to add item, we will not drop if they don't fit
	}

	//Logs items, got from (N)PC scripts [Lupus]
	if(log_config.pick > 0 ) {
		log_pick(sd, "N", 0, item_tmp.nameid, item_tmp.amount, &item_tmp);
	}
	//Logs

	push_val(st->stack,C_INT,1);
	return 0;
}

/*==========================================
 * gets a random item ID from an item group [Skotlex]
 * groupranditem group_num
 *------------------------------------------
 */
int buildin_grouprandomitem(struct script_state *st)
{
	int group;

	group = conv_num(st,& (st->stack->stack_data[st->start+2]));
	push_val(st->stack, C_INT, itemdb_searchrandomid(group));
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_makeitem(struct script_state *st)
{
	int nameid,amount,flag = 0;
	int x,y,m;
	char *mapname;
	struct item item_tmp;
	struct script_data *data;

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		nameid=512; //Apple Item ID
		if( item_data )
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	amount=conv_num(st,& (st->stack->stack_data[st->start+3]));
	mapname	=conv_str(st,& (st->stack->stack_data[st->start+4]));
	x	=conv_num(st,& (st->stack->stack_data[st->start+5]));
	y	=conv_num(st,& (st->stack->stack_data[st->start+6]));

	if(strcmp(mapname,"this")==0)
	{
		struct map_session_data *sd;
		sd = script_rid2sd(st);
		if (!sd) return 0; //Failed...
		m=sd->bl.m;
	} else
		m=map_mapname2mapid(mapname);

	if(nameid<0) { // ランダム
		nameid=itemdb_searchrandomid(-nameid);
		flag = 1;
	}

	if(nameid > 0) {
		memset(&item_tmp,0,sizeof(item_tmp));
		item_tmp.nameid=nameid;
		if(!flag)
			item_tmp.identify=1;
		else
			item_tmp.identify=!itemdb_isequip3(nameid);

		map_addflooritem(&item_tmp,amount,m,x,y,NULL,NULL,NULL,0);
	}

	return 0;
}
/*==========================================
 * script DELITEM command (fixed 2 bugs by Lupus, added deletion priority by Lupus)
 *------------------------------------------
 */
int buildin_delitem(struct script_state *st)
{
	int nameid=0,amount,i,important_item=0;
	struct map_session_data *sd;
	struct script_data *data;

	sd = script_rid2sd(st);

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		//nameid=512;
		if( item_data )
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	amount=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if (nameid<500 || amount<=0 ) {//by Lupus. Don't run FOR if u got wrong item ID or amount<=0
		//eprintf("wrong item ID or amount<=0 : delitem %i,\n",nameid,amount);
		return 0;
	}
	//1st pass
	//here we won't delete items with CARDS, named items but we count them
	for(i=0;i<MAX_INVENTORY;i++){
		//we don't delete wrong item or equipped item
		if(sd->status.inventory[i].nameid<=0 || sd->inventory_data[i] == NULL ||
		   sd->status.inventory[i].amount<=0 ||	sd->status.inventory[i].nameid!=nameid )
			continue;
		//1 egg uses 1 cell in the inventory. so it's ok to delete 1 pet / per cycle
		if(sd->inventory_data[i]->type==7 && sd->status.inventory[i].card[0] == (short)0xff00 && search_petDB_index(nameid, PET_EGG) >= 0 ){
			intif_delete_petdata( MakeDWord(sd->status.inventory[i].card[1], sd->status.inventory[i].card[2]) );
			//clear egg flag. so it won't be put in IMPORTANT items (eggs look like item with 2 cards ^_^)
			sd->status.inventory[i].card[1] = sd->status.inventory[i].card[0] = 0;
			//now this egg'll be deleted as a common unimportant item
		}
		//is this item important? does it have cards? or Player's name? or Refined/Upgraded
		if( sd->status.inventory[i].card[0] || sd->status.inventory[i].card[1] ||
			sd->status.inventory[i].card[2] || sd->status.inventory[i].card[3] || sd->status.inventory[i].refine) {
			//this is important item, count it
			important_item++;
			continue;
		}

		if(sd->status.inventory[i].amount>=amount){

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -amount, &sd->status.inventory[i]);
			}
			//Logs

			pc_delitem(sd,i,amount,0);
			return 0; //we deleted exact amount of items. now exit
		} else {
			amount-=sd->status.inventory[i].amount;

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -sd->status.inventory[i].amount, &sd->status.inventory[i]);
			}
			//Logs

			pc_delitem(sd,i,sd->status.inventory[i].amount,0);
		}
	}
	//2nd pass
	//now if there WERE items with CARDs/REFINED/NAMED... and if we still have to delete some items. we'll delete them finally
	if (important_item>0 && amount>0)
		for(i=0;i<MAX_INVENTORY;i++){
			//we don't delete wrong item
			if(sd->status.inventory[i].nameid<=0 || sd->inventory_data[i] == NULL ||
				sd->status.inventory[i].amount<=0 || sd->status.inventory[i].nameid!=nameid )
				continue;

			if(sd->status.inventory[i].amount>=amount){

				//Logs items, got from (N)PC scripts [Lupus]
				if(log_config.pick > 0 ) {
					log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -amount, &sd->status.inventory[i]);
				}
				//Logs

				pc_delitem(sd,i,amount,0);
				return 0; //we deleted exact amount of items. now exit
			} else {
				amount-=sd->status.inventory[i].amount;

				//Logs items, got from (N)PC scripts [Lupus]
				if(log_config.pick > 0 ) {
					log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -sd->status.inventory[i].amount, &sd->status.inventory[i]);
				}
				//Logs

				pc_delitem(sd,i,sd->status.inventory[i].amount,0);
			}
		}

	return 0;
}

/*==========================================
* advanced version of delitem [modified by Mihilion]
*------------------------------------------
*/
int buildin_delitem2(struct script_state *st)
{
	int nameid=0,amount,i=0;
	int iden,ref,attr,c1,c2,c3,c4;
	struct map_session_data *sd;
	struct script_data *data;

	sd = script_rid2sd(st);

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		//nameid=512;
		if( item_data )
			nameid=item_data->nameid;
	}else
		nameid=conv_num(st,data);

	amount=conv_num(st,& (st->stack->stack_data[st->start+3]));
	iden=conv_num(st,& (st->stack->stack_data[st->start+4]));
	ref=conv_num(st,& (st->stack->stack_data[st->start+5]));
	attr=conv_num(st,& (st->stack->stack_data[st->start+6]));
	c1=conv_num(st,& (st->stack->stack_data[st->start+7]));
	c2=conv_num(st,& (st->stack->stack_data[st->start+8]));
	c3=conv_num(st,& (st->stack->stack_data[st->start+9]));
	c4=conv_num(st,& (st->stack->stack_data[st->start+10]));

	if (nameid<500 || amount<=0 ) {//by Lupus. Don't run FOR if u got wrong item ID or amount<=0
	 //eprintf("wrong item ID or amount<=0 : delitem %i,\n",nameid,amount);
	 return 0;
	}

	for(i=0;i<MAX_INVENTORY;i++){
	//we don't delete wrong item or equipped item
		if(sd->status.inventory[i].nameid<=0 || sd->inventory_data[i] == NULL ||
			sd->status.inventory[i].amount<=0 || sd->status.inventory[i].nameid!=nameid ||
			sd->status.inventory[i].identify!=iden || sd->status.inventory[i].refine!=ref ||
			sd->status.inventory[i].attribute!=attr || sd->status.inventory[i].card[0]!=c1 ||
			sd->status.inventory[i].card[1]!=c2 || sd->status.inventory[i].card[2]!=c3 ||
			sd->status.inventory[i].card[3]!=c4)
			continue;
	//1 egg uses 1 cell in the inventory. so it's ok to delete 1 pet / per cycle
		if(sd->inventory_data[i]->type==7 && sd->status.inventory[i].card[0] == (short)0xff00 && search_petDB_index(nameid, PET_EGG) >= 0 ){
			intif_delete_petdata( MakeDWord(sd->status.inventory[i].card[1], sd->status.inventory[i].card[2]) );
			//clear egg flag. so it won't be put in IMPORTANT items (eggs look like item with 2 cards ^_^)
			sd->status.inventory[i].card[1] = sd->status.inventory[i].card[0] = 0;
			//now this egg'll be deleted as a common unimportant item
		}

		if(sd->status.inventory[i].amount>=amount){

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -amount, &sd->status.inventory[i]);
			}
			//Logs

			pc_delitem(sd,i,amount,0);
			return 0; //we deleted exact amount of items. now exit
		} else {
			amount-=sd->status.inventory[i].amount;

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -sd->status.inventory[i].amount, &sd->status.inventory[i]);
			}
			//Logs

			pc_delitem(sd,i,sd->status.inventory[i].amount,0);
		}
	}
	return 0;
}

/*==========================================
 * Enables/Disables use of items while in an NPC [Skotlex]
 *------------------------------------------
 */
int buildin_enableitemuse(struct script_state *st) {
	struct map_session_data *sd;
	sd=script_rid2sd(st);
	if (sd)
		sd->npc_item_flag = st->oid;
	return 0;
}

int buildin_disableitemuse(struct script_state *st) {
	struct map_session_data *sd;
	sd=script_rid2sd(st);
	if (sd)
		sd->npc_item_flag = 0;
	return 0;
}

/*==========================================
 *キャラ関係のパラメータ取得
 *------------------------------------------
 */
int buildin_readparam(struct script_state *st)
{
	int type;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if( st->end>st->start+3 )
		sd=map_nick2sd(conv_str(st,& (st->stack->stack_data[st->start+3])));
	else
	sd=script_rid2sd(st);

	if(sd==NULL){
		push_val(st->stack,C_INT,-1);
		return 0;
	}

	push_val(st->stack,C_INT,pc_readparam(sd,type));

	return 0;
}
/*==========================================
 *キャラ関係のID取得
 *------------------------------------------
 */
int buildin_getcharid(struct script_state *st)
{
	int num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if( st->end>st->start+3 )
		sd=map_nick2sd(conv_str(st,& (st->stack->stack_data[st->start+3])));
	else
	sd=script_rid2sd(st);
	if(sd==NULL){
		push_val(st->stack,C_INT,-1);
		return 0;
	}
	if(num==0)
		push_val(st->stack,C_INT,sd->status.char_id);
	if(num==1)
		push_val(st->stack,C_INT,sd->status.party_id);
	if(num==2)
		push_val(st->stack,C_INT,sd->status.guild_id);
	if(num==3)
		push_val(st->stack,C_INT,sd->status.account_id);
	return 0;
}
/*==========================================
 *指定IDのPT名取得
 *------------------------------------------
 */
char *buildin_getpartyname_sub(int party_id)
{
	struct party *p;

	p=NULL;
	p=party_search(party_id);

	if(p!=NULL){
		char *buf;
		buf=(char *)aCallocA(NAME_LENGTH,sizeof(char));
		memcpy(buf, p->name, NAME_LENGTH-1);
		return buf;
	}

	return 0;
}
int buildin_getpartyname(struct script_state *st)
{
	char *name;
	int party_id;

	party_id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	name=buildin_getpartyname_sub(party_id);
	if(name != NULL)
		push_str(st->stack,C_STR,(unsigned char *)name);
	else
		push_str(st->stack,C_CONSTSTR, (unsigned char *) "null");

	return 0;
}
/*==========================================
 *指定IDのPT人数とメンバーID取得
 *------------------------------------------
 */
int buildin_getpartymember(struct script_state *st)
{
	struct party *p;
	int i,j=0,type=0;

	p=NULL;
	p=party_search(conv_num(st,& (st->stack->stack_data[st->start+2])));

	if( st->end>st->start+3 )
 		type=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if(p!=NULL){
		for(i=0;i<MAX_PARTY;i++){
			if(p->member[i].account_id){
				switch (type) {
				case 2:
					mapreg_setreg(add_str((unsigned char *) "$@partymemberaid")+(j<<24),p->member[i].account_id);
					break;
				case 1:
					mapreg_setreg(add_str((unsigned char *) "$@partymembercid")+(j<<24),p->member[i].char_id);
					break;
				default:
					mapreg_setregstr(add_str((unsigned char *) "$@partymembername$")+(j<<24),p->member[i].name);
				}
				j++;
			}
		}
	}
	mapreg_setreg(add_str((unsigned char *) "$@partymembercount"),j);

	return 0;
}
/*==========================================
 *指定IDのギルド名取得
 *------------------------------------------
 */
char *buildin_getguildname_sub(int guild_id)
{
	struct guild *g=NULL;
	g=guild_search(guild_id);

	if(g!=NULL){
		char *buf;
		buf=(char *)aCallocA(NAME_LENGTH,sizeof(char));
		memcpy(buf, g->name, NAME_LENGTH-1);
		return buf;
	}
	return NULL;
}
int buildin_getguildname(struct script_state *st)
{
	char *name;
	int guild_id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	name=buildin_getguildname_sub(guild_id);
	if(name != NULL)
		push_str(st->stack,C_STR,(unsigned char *) name);
	else
		push_str(st->stack,C_CONSTSTR,(unsigned char *) "null");
	return 0;
}

/*==========================================
 *指定IDのGuildMaster名取得
 *------------------------------------------
 */
char *buildin_getguildmaster_sub(int guild_id)
{
	struct guild *g=NULL;
	g=guild_search(guild_id);

	if(g!=NULL){
		char *buf;
		buf=(char *)aCallocA(NAME_LENGTH,sizeof(char));
		memcpy(buf, g->master, NAME_LENGTH-1);
		return buf;
	}

	return 0;
}
int buildin_getguildmaster(struct script_state *st)
{
	char *master;
	int guild_id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	master=buildin_getguildmaster_sub(guild_id);
	if(master!=0)
		push_str(st->stack,C_STR,(unsigned char *) master);
	else
		push_str(st->stack,C_CONSTSTR,(unsigned char *) "null");
	return 0;
}

int buildin_getguildmasterid(struct script_state *st)
{
	char *master;
	struct map_session_data *sd=NULL;
	int guild_id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	master=buildin_getguildmaster_sub(guild_id);
	if(master!=0){
		if((sd=map_nick2sd(master)) == NULL){
			push_val(st->stack,C_INT,0);
			return 0;
		}
		push_val(st->stack,C_INT,sd->status.char_id);
	}else{
		push_val(st->stack,C_INT,0);
	}
	return 0;
}

/*==========================================
 * キャラクタの名前
 *------------------------------------------
 */
int buildin_strcharinfo(struct script_state *st)
{
	struct map_session_data *sd;
	char *buf;
	int num;

	sd=script_rid2sd(st);
	if (!sd) { //Avoid crashing....
		push_str(st->stack,C_CONSTSTR,(unsigned char *) "");
		return 0;
	}
	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	switch(num){
		case 0:
			buf=(char *)aCallocA(NAME_LENGTH,sizeof(char));
			memcpy(buf, sd->status.name, NAME_LENGTH-1);
			push_str(st->stack,C_STR,(unsigned char *) buf);
			break;
		case 1:
			buf=buildin_getpartyname_sub(sd->status.party_id);
			if(buf!=0)
				push_str(st->stack,C_STR,(unsigned char *) buf);
			else
				push_str(st->stack,C_CONSTSTR,(unsigned char *) "");
			break;
		case 2:
			buf=buildin_getguildname_sub(sd->status.guild_id);
			if(buf != NULL)
				push_str(st->stack,C_STR,(unsigned char *) buf);
			else
				push_str(st->stack,C_CONSTSTR,(unsigned char *) "");
			break;
		default:
			ShowWarning("buildin_strcharinfo: unknown parameter.");
			push_str(st->stack,C_CONSTSTR,(unsigned char *) "");
			break;
	}

	return 0;
}

unsigned int equip[10]={0x0100,0x0010,0x0020,0x0002,0x0004,0x0040,0x0008,0x0080,0x0200,0x0001};

/*==========================================
 * GetEquipID(Pos);     Pos: 1-10
 *------------------------------------------
 */
int buildin_getequipid(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;
	struct item_data* item;

	sd=script_rid2sd(st);
	if(sd == NULL)
	{
		ShowError("getequipid: sd == NULL\n");
		return 0;
	}
	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0){
		item=sd->inventory_data[i];
		if(item)
			push_val(st->stack,C_INT,item->nameid);
		else
			push_val(st->stack,C_INT,0);
	}else{
		push_val(st->stack,C_INT,-1);
	}
	return 0;
}

/*==========================================
 * 装備名文字列（精錬メニュー用）
 *------------------------------------------
 */
int buildin_getequipname(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;
	struct item_data* item;
	char *buf;

	buf=(char *)aCallocA(64,sizeof(char));
	sd=script_rid2sd(st);
	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0){
		item=sd->inventory_data[i];
		if(item)
			sprintf(buf,"%s-[%s]",pos[num-1],item->jname);
		else
			sprintf(buf,"%s-[%s]",pos[num-1],pos[10]);
	}else{
		sprintf(buf,"%s-[%s]",pos[num-1],pos[10]);
	}
	push_str(st->stack,C_STR,(unsigned char *) buf);

	return 0;
}

/*==========================================
 * getbrokenid [Valaris]
 *------------------------------------------
 */
int buildin_getbrokenid(struct script_state *st)
{
	int i,num,id=0,brokencounter=0;
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	for(i=0; i<MAX_INVENTORY; i++) {
		if(sd->status.inventory[i].attribute==1){
				brokencounter++;
				if(num==brokencounter){
					id=sd->status.inventory[i].nameid;
					break;
				}
		}
	}

	push_val(st->stack,C_INT,id);

	return 0;
}

/*==========================================
 * repair [Valaris]
 *------------------------------------------
 */
int buildin_repair(struct script_state *st)
{
	int i,num;
	int repaircounter=0;
	struct map_session_data *sd;


	sd=script_rid2sd(st);

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	for(i=0; i<MAX_INVENTORY; i++) {
		if(sd->status.inventory[i].attribute==1){
				repaircounter++;
				if(num==repaircounter){
					sd->status.inventory[i].attribute=0;
					clif_equiplist(sd);
					clif_produceeffect(sd, 0, sd->status.inventory[i].nameid);
					clif_misceffect(&sd->bl, 3);
					clif_displaymessage(sd->fd,"Item has been repaired.");
					break;
				}
		}
	}

	return 0;
}

/*==========================================
 * 装備チェック
 *------------------------------------------
 */
int buildin_getequipisequiped(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);

	if ((num - 1)  >= (sizeof(equip) / sizeof(equip[0])))
		i = -1;
	else
		i=pc_checkequip(sd,equip[num-1]);

        if(i >= 0)
          push_val(st->stack,C_INT,1);
        else
          push_val(st->stack,C_INT,0);

	return 0;
}

/*==========================================
 * 装備品精錬可能チェック
 *------------------------------------------
 */
int buildin_getequipisenableref(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0 && num<7 && sd->inventory_data[i] && !sd->inventory_data[i]->flag.no_refine)
	{
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}

	return 0;
}

/*==========================================
 * 装備品鑑定チェック
 *------------------------------------------
 */
int buildin_getequipisidentify(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0)
		push_val(st->stack,C_INT,sd->status.inventory[i].identify);
	else
		push_val(st->stack,C_INT,0);

	return 0;
}

/*==========================================
 * 装備品精錬度
 *------------------------------------------
 */
int buildin_getequiprefinerycnt(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0)
		push_val(st->stack,C_INT,sd->status.inventory[i].refine);
	else
		push_val(st->stack,C_INT,0);

	return 0;
}

/*==========================================
 * 装備品武器LV
 *------------------------------------------
 */
int buildin_getequipweaponlv(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0 && sd->inventory_data[i])
		push_val(st->stack,C_INT,sd->inventory_data[i]->wlv);
	else
		push_val(st->stack,C_INT,0);

	return 0;
}

/*==========================================
 * 装備品精錬成功率
 *------------------------------------------
 */
int buildin_getequippercentrefinery(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0 && sd->status.inventory[i].nameid && sd->status.inventory[i].refine < MAX_REFINE)
		push_val(st->stack,C_INT,percentrefinery[itemdb_wlv(sd->status.inventory[i].nameid)][(int)sd->status.inventory[i].refine]);
	else
		push_val(st->stack,C_INT,0);

	return 0;
}

/*==========================================
 * 精錬成功
 *------------------------------------------
 */
int buildin_successrefitem(struct script_state *st)
{
	int i,num,ep;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0) {
		ep=sd->status.inventory[i].equip;

		if(log_config.refine > 0)
			log_refine(sd, i, 1);

		//Logs items, got from (N)PC scripts [Lupus]
		if(log_config.pick > 0 ) {
			log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -1, &sd->status.inventory[i]);
		}
		//Logs

		sd->status.inventory[i].refine++;
		pc_unequipitem(sd,i,2);

		clif_refine(sd->fd,sd,0,i,sd->status.inventory[i].refine);
		clif_delitem(sd,i,1);

		//Logs items, got from (N)PC scripts [Lupus]
		if(log_config.pick > 0 ) {
			log_pick(sd, "N", 0, sd->status.inventory[i].nameid, 1, &sd->status.inventory[i]);
		}
		//Logs

		clif_additem(sd,i,1,0);
		pc_equipitem(sd,i,ep);
		clif_misceffect(&sd->bl,3);
		if(sd->status.inventory[i].refine == 10 && sd->status.inventory[i].card[0] == 0x00ff && sd->char_id == MakeDWord(sd->status.inventory[i].card[2],sd->status.inventory[i].card[3])){ // Fame point system [DracoRPG]
	 		switch (sd->inventory_data[i]->wlv){
				case 1:
					pc_addfame(sd,1); // Success to refine to +10 a lv1 weapon you forged = +1 fame point
					break;
				case 2:
					pc_addfame(sd,25); // Success to refine to +10 a lv2 weapon you forged = +25 fame point
					break;
				case 3:
					pc_addfame(sd,1000); // Success to refine to +10 a lv3 weapon you forged = +1000 fame point
					break;
	 	 	 }
		}
	}

	return 0;
}

/*==========================================
 * 精錬失敗
 *------------------------------------------
 */
int buildin_failedrefitem(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0) {
		if(log_config.refine > 0)
			log_refine(sd, i, 0);

		//Logs items, got from (N)PC scripts [Lupus]
		if(log_config.pick > 0 ) {
			log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -1, &sd->status.inventory[i]);
		}
		//Logs

		sd->status.inventory[i].refine = 0;
		pc_unequipitem(sd,i,3);
		// 精錬失敗エフェクトのパケット
		clif_refine(sd->fd,sd,1,i,sd->status.inventory[i].refine);

		pc_delitem(sd,i,1,0);
		// 他の人にも失敗を通知
		clif_misceffect(&sd->bl,2);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_statusup(struct script_state *st)
{
	int type;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	pc_statusup(sd,type);

	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_statusup2(struct script_state *st)
{
	int type,val;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	val=conv_num(st,& (st->stack->stack_data[st->start+3]));
	sd=script_rid2sd(st);
	pc_statusup2(sd,type,val);

	return 0;
}
/*==========================================
 * 装備品による能力値ボーナス
 *------------------------------------------
 */
int buildin_bonus(struct script_state *st)
{
	int type,val;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	val=conv_num(st,& (st->stack->stack_data[st->start+3]));
	sd=script_rid2sd(st);
	pc_bonus(sd,type,val);

	return 0;
}
/*==========================================
 * 装備品による能力値ボーナス
 *------------------------------------------
 */
int buildin_bonus2(struct script_state *st)
{
	int type,type2,val;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	type2=conv_num(st,& (st->stack->stack_data[st->start+3]));
	val=conv_num(st,& (st->stack->stack_data[st->start+4]));
	sd=script_rid2sd(st);
	pc_bonus2(sd,type,type2,val);

	return 0;
}
/*==========================================
 * 装備品による能力値ボーナス
 *------------------------------------------
 */
int buildin_bonus3(struct script_state *st)
{
	int type,type2,type3,val;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	type2=conv_num(st,& (st->stack->stack_data[st->start+3]));
	type3=conv_num(st,& (st->stack->stack_data[st->start+4]));
	val=conv_num(st,& (st->stack->stack_data[st->start+5]));
	sd=script_rid2sd(st);
	pc_bonus3(sd,type,type2,type3,val);

	return 0;
}

int buildin_bonus4(struct script_state *st)
{
	int type,type2,type3,type4,val;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	type2=conv_num(st,& (st->stack->stack_data[st->start+3]));
	type3=conv_num(st,& (st->stack->stack_data[st->start+4]));
	type4=conv_num(st,& (st->stack->stack_data[st->start+5]));
	val=conv_num(st,& (st->stack->stack_data[st->start+6]));
	sd=script_rid2sd(st);
	pc_bonus4(sd,type,type2,type3,type4,val);

	return 0;
}
/*==========================================
 * スキル所得
 *------------------------------------------
 */
int buildin_skill(struct script_state *st)
{
	int id,level,flag=1;
	struct map_session_data *sd;

	id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	level=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if( st->end>st->start+4 )
		flag=conv_num(st,&(st->stack->stack_data[st->start+4]) );
	sd=script_rid2sd(st);
	pc_skill(sd,id,level,flag);

	return 0;
}

// add x levels of skill (stackable) [Valaris]
int buildin_addtoskill(struct script_state *st)
{
	int id,level,flag=2;
	struct map_session_data *sd;

	id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	level=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if( st->end>st->start+4 )
		flag=conv_num(st,&(st->stack->stack_data[st->start+4]) );
	sd=script_rid2sd(st);
	pc_skill(sd,id,level,flag);

	return 0;
}

/*==========================================
 * ギルドスキル取得
 *------------------------------------------
 */
int buildin_guildskill(struct script_state *st)
{
	int id,level,flag=0;
	struct map_session_data *sd;
	int i=0;

	id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	level=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if( st->end>st->start+4 )
		flag=conv_num(st,&(st->stack->stack_data[st->start+4]) );
	sd=script_rid2sd(st);
	for(i=0;i<level;i++)
		guild_skillup(sd,id,flag);

	return 0;
}
/*==========================================
 * スキルレベル所得
 *------------------------------------------
 */
int buildin_getskilllv(struct script_state *st)
{
	int id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	push_val(st->stack,C_INT, pc_checkskill( script_rid2sd(st) ,id) );
	return 0;
}
/*==========================================
 * getgdskilllv(Guild_ID, Skill_ID);
 * skill_id = 10000 : GD_APPROVAL
 *            10001 : GD_KAFRACONTRACT
 *            10002 : GD_GUARDIANRESEARCH
 *            10003 : GD_GUARDUP
 *            10004 : GD_EXTENSION
 *------------------------------------------
 */
int buildin_getgdskilllv(struct script_state *st)
{
        int guild_id=conv_num(st,& (st->stack->stack_data[st->start+2]));
        int skill_id=conv_num(st,& (st->stack->stack_data[st->start+3]));
        struct guild *g=guild_search(guild_id);
	push_val(st->stack,C_INT, (g==NULL)?-1:guild_checkskill(g,skill_id) );
	return 0;
/*
	struct map_session_data *sd=NULL;
	struct guild *g=NULL;
	int skill_id;

	skill_id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	if(sd && sd->status.guild_id > 0) g=guild_search(sd->status.guild_id);
	if(sd && g) {
		push_val(st->stack,C_INT, guild_checkskill(g,skill_id+9999) );
	} else {
		push_val(st->stack,C_INT,-1);
	}
	return 0;
*/
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_basicskillcheck(struct script_state *st)
{
	push_val(st->stack,C_INT, battle_config.basic_skill_check);
	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_getgmlevel(struct script_state *st)
{
	push_val(st->stack,C_INT, pc_isGM(script_rid2sd(st)));
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_end(struct script_state *st)
{
	st->state = END;
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_checkoption(struct script_state *st)
{
	int type;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);

	if(sd->sc.option & type){
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}

	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_checkoption1(struct script_state *st)
{
	int type;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);

	if(sd->sc.opt1 & type){
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}

	return 0;
}
/*==========================================
 *
 *------------------------------------------
 */
int buildin_checkoption2(struct script_state *st)
{
	int type;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);

	if(sd->sc.opt2 & type){
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_setoption(struct script_state *st)
{
	int type;
	struct map_session_data *sd;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	pc_setoption(sd,type);

	return 0;
}

/*==========================================
 * Checkcart [Valaris]
 *------------------------------------------
 */

int buildin_checkcart(struct script_state *st)
{
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	if(pc_iscarton(sd)){
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}
	return 0;
}

/*==========================================
 * カートを付ける
 *------------------------------------------
 */
int buildin_setcart(struct script_state *st)
{
	struct map_session_data *sd;

	sd=script_rid2sd(st);
	pc_setcart(sd,1);

	return 0;
}

/*==========================================
 * checkfalcon [Valaris]
 *------------------------------------------
 */

int buildin_checkfalcon(struct script_state *st)
{
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	if(pc_isfalcon(sd)){
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}

	return 0;
}


/*==========================================
 * 鷹を付ける
 *------------------------------------------
 */
int buildin_setfalcon(struct script_state *st)
{
	struct map_session_data *sd;

	sd=script_rid2sd(st);
	pc_setfalcon(sd);

	return 0;
}

/*==========================================
 * Checkcart [Valaris]
 *------------------------------------------
 */

int buildin_checkriding(struct script_state *st)
{
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	if(pc_isriding(sd)){
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}

	return 0;
}


/*==========================================
 * ペコペコ乗り
 *------------------------------------------
 */
int buildin_setriding(struct script_state *st)
{
	struct map_session_data *sd;

	sd=script_rid2sd(st);
	pc_setriding(sd);

	return 0;
}

/*==========================================
 *	セーブポイントの保存
 *------------------------------------------
 */
int buildin_savepoint(struct script_state *st)
{
	int x,y;
	short map;
	char *str;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));
	map = mapindex_name2id(str);
	if (map)
		pc_setsavepoint(script_rid2sd(st),map,x,y);
	return 0;
}

/*==========================================
 * GetTimeTick(0: System Tick, 1: Time Second Tick)
 *------------------------------------------
 */
int buildin_gettimetick(struct script_state *st)	/* Asgard Version */
{
	int type;
	time_t timer;
	struct tm *t;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));

	switch(type){
	case 2:
		//type 2:(Get the number of seconds elapsed since 00:00 hours, Jan 1, 1970 UTC
		//        from the system clock.)
		push_val(st->stack,C_INT,(int)time(NULL));
		break;
	case 1:
		//type 1:(Second Ticks: 0-86399, 00:00:00-23:59:59)
		time(&timer);
		t=localtime(&timer);
		push_val(st->stack,C_INT,((t->tm_hour)*3600+(t->tm_min)*60+t->tm_sec));
		break;
	case 0:
	default:
		//type 0:(System Ticks)
		push_val(st->stack,C_INT,gettick());
		break;
	}
	return 0;
}

/*==========================================
 * GetTime(Type);
 * 1: Sec     2: Min     3: Hour
 * 4: WeekDay     5: MonthDay     6: Month
 * 7: Year
 *------------------------------------------
 */
int buildin_gettime(struct script_state *st)	/* Asgard Version */
{
	int type;
	time_t timer;
	struct tm *t;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));

	time(&timer);
	t=localtime(&timer);

	switch(type){
	case 1://Sec(0~59)
		push_val(st->stack,C_INT,t->tm_sec);
		break;
	case 2://Min(0~59)
		push_val(st->stack,C_INT,t->tm_min);
		break;
	case 3://Hour(0~23)
		push_val(st->stack,C_INT,t->tm_hour);
		break;
	case 4://WeekDay(0~6)
		push_val(st->stack,C_INT,t->tm_wday);
		break;
	case 5://MonthDay(01~31)
		push_val(st->stack,C_INT,t->tm_mday);
		break;
	case 6://Month(01~12)
		push_val(st->stack,C_INT,t->tm_mon+1);
		break;
	case 7://Year(20xx)
		push_val(st->stack,C_INT,t->tm_year+1900);
		break;
	case 8://Year Day(01~366)
		push_val(st->stack,C_INT,t->tm_yday+1);
		break;
	default://(format error)
		push_val(st->stack,C_INT,-1);
		break;
	}
	return 0;
}

/*==========================================
 * GetTimeStr("TimeFMT", Length);
 *------------------------------------------
 */
int buildin_gettimestr(struct script_state *st)
{
	char *tmpstr;
	char *fmtstr;
	int maxlen;
	time_t now = time(NULL);

	fmtstr=conv_str(st,& (st->stack->stack_data[st->start+2]));
	maxlen=conv_num(st,& (st->stack->stack_data[st->start+3]));

	tmpstr=(char *)aCallocA(maxlen+1,sizeof(char));
	strftime(tmpstr,maxlen,fmtstr,localtime(&now));
	tmpstr[maxlen]='\0';

	push_str(st->stack,C_STR,(unsigned char *) tmpstr);
	return 0;
}

/*==========================================
 * カプラ倉庫を開く
 *------------------------------------------
 */
int buildin_openstorage(struct script_state *st)
{
	storage_storageopen(script_rid2sd(st));
	return 0;
}

int buildin_guildopenstorage(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	int ret;
	ret = storage_guild_storageopen(sd);
	push_val(st->stack,C_INT,ret);
	return 0;
}

/*==========================================
 * アイテムによるスキル発動
 *------------------------------------------
 */
int buildin_itemskill(struct script_state *st)
{
	int id,lv;
	char *str;
	struct map_session_data *sd=script_rid2sd(st);

	id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	lv=conv_num(st,& (st->stack->stack_data[st->start+3]));
	str=conv_str(st,& (st->stack->stack_data[st->start+4]));

	// 詠唱中にスキルアイテムは使用できない
	if(sd->ud.skilltimer != -1)
		return 0;

	sd->skillitem=id;
	sd->skillitemlv=lv;
	clif_item_skill(sd,id,lv,str);
	return 0;
}
/*==========================================
 * アイテム作成
 *------------------------------------------
 */
int buildin_produce(struct script_state *st)
{
	int trigger;
	struct map_session_data *sd=script_rid2sd(st);

	trigger=conv_num(st,& (st->stack->stack_data[st->start+2]));
	clif_skill_produce_mix_list(sd, trigger);
	return 0;
}
/*==========================================
 * NPCでペット作る
 *------------------------------------------
 */
int buildin_makepet(struct script_state *st)
{
	struct map_session_data *sd = script_rid2sd(st);
	struct script_data *data;
	int id,pet_id;

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);

	id=conv_num(st,data);

	pet_id = search_petDB_index(id, PET_CLASS);

	if (pet_id < 0)
		pet_id = search_petDB_index(id, PET_EGG);
	if (pet_id >= 0 && sd) {
		sd->catch_target_class = pet_db[pet_id].class_;
		intif_create_pet(
			sd->status.account_id, sd->status.char_id,
			(short)pet_db[pet_id].class_, (short)mob_db(pet_db[pet_id].class_)->lv,
			(short)pet_db[pet_id].EggID, 0, (short)pet_db[pet_id].intimate,
			100, 0, 1, pet_db[pet_id].jname);
	}

	return 0;
}
/*==========================================
 * NPCで経験値上げる
 *------------------------------------------
 */
int buildin_getexp(struct script_state *st)
{
	struct map_session_data *sd = script_rid2sd(st);
	int base=0,job=0;

	base=conv_num(st,& (st->stack->stack_data[st->start+2]));
	job =conv_num(st,& (st->stack->stack_data[st->start+3]));
	if(base<0 || job<0)
		return 0;
	if(sd)
		pc_gainexp(sd,base,job);

	return 0;
}

/*==========================================
 * Gain guild exp [Celest]
 *------------------------------------------
 */
int buildin_guildgetexp(struct script_state *st)
{
	struct map_session_data *sd = script_rid2sd(st);
	int exp;

	exp = conv_num(st,& (st->stack->stack_data[st->start+2]));
	if(exp < 0)
		return 0;
	if(sd && sd->status.guild_id > 0)
		guild_getexp (sd, exp);

	return 0;
}

/*==========================================
 * Changes the guild master of a guild [Skotlex]
 *------------------------------------------
 */
int buildin_guildchangegm(struct script_state *st)
{
	struct map_session_data *sd;
	int guild_id;
	char *name;

	guild_id = conv_num(st,& (st->stack->stack_data[st->start+2]));
	name = conv_str(st,& (st->stack->stack_data[st->start+3]));
	sd=map_nick2sd(name);

	if (!sd)
		push_val(st->stack,C_INT,0);
	else
		push_val(st->stack,C_INT,guild_gm_change(guild_id, sd));

	return 0;
}

/*==========================================
 * モンスター発生
 *------------------------------------------
 */
int buildin_monster(struct script_state *st)
{
	int class_,amount,x,y;
	char *str,*map,*event="";

	map	=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x	=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y	=conv_num(st,& (st->stack->stack_data[st->start+4]));
	str	=conv_str(st,& (st->stack->stack_data[st->start+5]));
	class_=conv_num(st,& (st->stack->stack_data[st->start+6]));
	amount=conv_num(st,& (st->stack->stack_data[st->start+7]));
	if( st->end>st->start+8 )
		event=conv_str(st,& (st->stack->stack_data[st->start+8]));

	if (class_ >= 0 && !mobdb_checkid(class_)) {
		ShowWarning("buildin_monster: Attempted to spawn non-existing monster class %d\n", class_);
		return 1;
	}
	mob_once_spawn(map_id2sd(st->rid),map,x,y,str,class_,amount,event);
	return 0;
}
/*==========================================
 * モンスター発生
 *------------------------------------------
 */
int buildin_areamonster(struct script_state *st)
{
	int class_,amount,x0,y0,x1,y1;
	char *str,*map,*event="";

	map	=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x0	=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y0	=conv_num(st,& (st->stack->stack_data[st->start+4]));
	x1	=conv_num(st,& (st->stack->stack_data[st->start+5]));
	y1	=conv_num(st,& (st->stack->stack_data[st->start+6]));
	str	=conv_str(st,& (st->stack->stack_data[st->start+7]));
	class_=conv_num(st,& (st->stack->stack_data[st->start+8]));
	amount=conv_num(st,& (st->stack->stack_data[st->start+9]));
	if( st->end>st->start+10 )
		event=conv_str(st,& (st->stack->stack_data[st->start+10]));

	mob_once_spawn_area(map_id2sd(st->rid),map,x0,y0,x1,y1,str,class_,amount,event);
	return 0;
}
/*==========================================
 * モンスター削除
 *------------------------------------------
 */
int buildin_killmonster_sub(struct block_list *bl,va_list ap)
{
	TBL_MOB* md = (TBL_MOB*)bl;
	char *event=va_arg(ap,char *);
	int allflag=va_arg(ap,int);

	if(!allflag){
		if(strcmp(event,md->npc_event)==0)
			unit_remove_map(bl,1);
		return 0;
	}else{
		if(!md->spawn)
			unit_remove_map(bl,1);
		return 0;
	}
	return 0;
}
int buildin_killmonster(struct script_state *st)
{
	char *mapname,*event;
	int m,allflag=0;
	mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	event=conv_str(st,& (st->stack->stack_data[st->start+3]));
	if(strcmp(event,"All")==0)
		allflag = 1;

	if( (m=map_mapname2mapid(mapname))<0 )
		return 0;
	map_foreachinmap(buildin_killmonster_sub, m, BL_MOB, event ,allflag);
	return 0;
}

int buildin_killmonsterall_sub(struct block_list *bl,va_list ap)
{
	unit_remove_map(bl,1);
	return 0;
}
int buildin_killmonsterall(struct script_state *st)
{
	char *mapname;
	int m;
	mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));

	if( (m=map_mapname2mapid(mapname))<0 )
		return 0;
	map_foreachinmap(buildin_killmonsterall_sub,
		m,BL_MOB);
	return 0;
}

/*==========================================
 * Creates a clone of a player.
 * clone map, x, y, event, char_id, master_id, mode, flag, duration
 *------------------------------------------
 */
int buildin_clone(struct script_state *st) {
	struct map_session_data *sd, *msd=NULL;
	int char_id,master_id=0,x,y, mode = 0, flag = 0;
	unsigned int duration = 0;
	char *map,*event="";

	map=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));
	event=conv_str(st,& (st->stack->stack_data[st->start+5]));
	char_id=conv_num(st,& (st->stack->stack_data[st->start+6]));

	if( st->end>st->start+7 )
		master_id=conv_num(st,& (st->stack->stack_data[st->start+7]));

	if( st->end>st->start+8 )
		mode=conv_num(st,& (st->stack->stack_data[st->start+8]));

	if( st->end>st->start+9 )
		flag=conv_num(st,& (st->stack->stack_data[st->start+9]));

	if( st->end>st->start+10 )
		duration=conv_num(st,& (st->stack->stack_data[st->start+10]));

	sd = map_charid2sd(char_id);
	if (master_id) {
		msd = map_charid2sd(master_id);
		if (msd)
			master_id = msd->bl.id;
		else
			master_id = 0;
	}
	if (sd) //Return ID of newly crafted clone.
		push_val(st->stack,C_INT,mob_clone_spawn(sd, map, x, y, event, master_id, mode, flag, 1000*duration));
	else //Failed to create clone.
		push_val(st->stack,C_INT,0);

	return 0;
}
/*==========================================
 * イベント実行
 *------------------------------------------
 */
int buildin_doevent(struct script_state *st)
{
	char *event;
	event=conv_str(st,& (st->stack->stack_data[st->start+2]));
	npc_event(map_id2sd(st->rid),event,0);
	return 0;
}
/*==========================================
 * NPC主体イベント実行
 *------------------------------------------
 */
int buildin_donpcevent(struct script_state *st)
{
	char *event;
	event=conv_str(st,& (st->stack->stack_data[st->start+2]));
	npc_event_do(event);
	return 0;
}
/*==========================================
 * イベントタイマー追加
 *------------------------------------------
 */
int buildin_addtimer(struct script_state *st)
{
	char *event;
	int tick;
	tick=conv_num(st,& (st->stack->stack_data[st->start+2]));
	event=conv_str(st,& (st->stack->stack_data[st->start+3]));
	pc_addeventtimer(script_rid2sd(st),tick,event);
	return 0;
}
/*==========================================
 * イベントタイマー削除
 *------------------------------------------
 */
int buildin_deltimer(struct script_state *st)
{
	char *event;
	event=conv_str(st,& (st->stack->stack_data[st->start+2]));
	pc_deleventtimer(script_rid2sd(st),event);
	return 0;
}
/*==========================================
 * イベントタイマーのカウント値追加
 *------------------------------------------
 */
int buildin_addtimercount(struct script_state *st)
{
	char *event;
	int tick;
	event=conv_str(st,& (st->stack->stack_data[st->start+2]));
	tick=conv_num(st,& (st->stack->stack_data[st->start+3]));
	pc_addeventtimercount(script_rid2sd(st),event,tick);
	return 0;
}

/*==========================================
 * NPCタイマー初期化
 *------------------------------------------
 */
int buildin_initnpctimer(struct script_state *st)
{
	struct npc_data *nd;
	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	npc_settimerevent_tick(nd,0);
	npc_timerevent_start(nd, st->rid);
	return 0;
}
/*==========================================
 * NPCタイマー開始
 *------------------------------------------
 */
int buildin_startnpctimer(struct script_state *st)
{
	struct npc_data *nd;
	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	npc_timerevent_start(nd, st->rid);
	return 0;
}
/*==========================================
 * NPCタイマー停止
 *------------------------------------------
 */
int buildin_stopnpctimer(struct script_state *st)
{
	struct npc_data *nd;
	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	npc_timerevent_stop(nd);
	return 0;
}
/*==========================================
 * NPCタイマー情報所得
 *------------------------------------------
 */
int buildin_getnpctimer(struct script_state *st)
{
	struct npc_data *nd;
	int type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	int val=0;
	if( st->end > st->start+3 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+3])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	switch(type){
	case 0: val=npc_gettimerevent_tick(nd); break;
	case 1: val= (nd->u.scr.nexttimer>=0); break;
	case 2: val= nd->u.scr.timeramount; break;
	}
	push_val(st->stack,C_INT,val);
	return 0;
}
/*==========================================
 * NPCタイマー値設定
 *------------------------------------------
 */
int buildin_setnpctimer(struct script_state *st)
{
	int tick;
	struct npc_data *nd;
	tick=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if( st->end > st->start+3 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+3])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	npc_settimerevent_tick(nd,tick);
	return 0;
}

/*==========================================
 * attaches the player rid to the timer [Celest]
 *------------------------------------------
 */
int buildin_attachnpctimer(struct script_state *st)
{
	struct map_session_data *sd;
	struct npc_data *nd;

	nd=(struct npc_data *)map_id2bl(st->oid);
	if( st->end > st->start+2 ) {
		char *name = conv_str(st,& (st->stack->stack_data[st->start+2]));
		sd=map_nick2sd(name);
	} else {
		sd = script_rid2sd(st);
	}

	if (sd==NULL)
		return 0;

	nd->u.scr.rid = sd->bl.id;
	return 0;
}

/*==========================================
 * detaches a player rid from the timer [Celest]
 *------------------------------------------
 */
int buildin_detachnpctimer(struct script_state *st)
{
	struct npc_data *nd;
	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	nd->u.scr.rid = 0;
	return 0;
}

/*==========================================
 * To avoid "player not attached" script errors, this function is provided,
 * it checks if there is a player attached to the current script. [Skotlex]
 * If no, returns 0, if yes, returns the char_id of the attached player.
 *------------------------------------------
 */
int buildin_playerattached(struct script_state *st)
{
	struct map_session_data *sd;
	if (st->rid == 0 || (sd = map_id2sd(st->rid)) == NULL)
		push_val(st->stack,C_INT,0);
	else
		push_val(st->stack,C_INT,st->rid);
	return 0;
}

/*==========================================
 * 天の声アナウンス
 *------------------------------------------
 */
int buildin_announce(struct script_state *st)
{
	char *str, *color=NULL;
	int flag;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	flag=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if (st->end>st->start+4)
		color=conv_str(st,& (st->stack->stack_data[st->start+4]));

	if(flag&0x0f){
		struct block_list *bl=(flag&0x08)? map_id2bl(st->oid) :
			(struct block_list *)script_rid2sd(st);
		if (color)
			clif_announce(bl,str,(int)strlen(str)+1, strtol(color, (char **)NULL, 0),flag);
		else
			clif_GMmessage(bl,str,(int)strlen(str)+1,flag);
	}else {
		if (color)
			intif_announce(str,(int)strlen(str)+1, strtol(color, (char **)NULL, 0), flag);
		else
			intif_GMmessage(str,(int)strlen(str)+1,flag);
	}
	return 0;
}
/*==========================================
 * 天の声アナウンス（特定マップ）
 *------------------------------------------
 */
int buildin_mapannounce_sub(struct block_list *bl,va_list ap)
{
	char *str, *color;
	int len,flag;
	str=va_arg(ap,char *);
	len=va_arg(ap,int);
	flag=va_arg(ap,int);
	color=va_arg(ap,char *);
	if (color)
		clif_announce(bl,str,len, strtol(color, (char **)NULL, 0), flag|3);
	else
		clif_GMmessage(bl,str,len,flag|3);
	return 0;
}
int buildin_mapannounce(struct script_state *st)
{
	char *mapname,*str, *color=NULL;
	int flag,m;

	mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	str=conv_str(st,& (st->stack->stack_data[st->start+3]));
	flag=conv_num(st,& (st->stack->stack_data[st->start+4]));
	if (st->end>st->start+5)
		color=conv_str(st,& (st->stack->stack_data[st->start+5]));

	if( (m=map_mapname2mapid(mapname))<0 )
		return 0;

	map_foreachinmap(buildin_mapannounce_sub,
		m, BL_PC, str,strlen(str)+1,flag&0x10, color);
	return 0;
}
/*==========================================
 * 天の声アナウンス（特定エリア）
 *------------------------------------------
 */
int buildin_areaannounce(struct script_state *st)
{
	char *map,*str,*color=NULL;
	int flag,m;
	int x0,y0,x1,y1;

	map=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x0=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y0=conv_num(st,& (st->stack->stack_data[st->start+4]));
	x1=conv_num(st,& (st->stack->stack_data[st->start+5]));
	y1=conv_num(st,& (st->stack->stack_data[st->start+6]));
	str=conv_str(st,& (st->stack->stack_data[st->start+7]));
	flag=conv_num(st,& (st->stack->stack_data[st->start+8]));
	if (st->end>st->start+9)
		color=conv_str(st,& (st->stack->stack_data[st->start+9]));

	if( (m=map_mapname2mapid(map))<0 )
		return 0;

	map_foreachinarea(buildin_mapannounce_sub,
		m,x0,y0,x1,y1,BL_PC, str,strlen(str)+1,flag&0x10, color);
	return 0;
}

/*==========================================
 * ユーザー数所得
 *------------------------------------------
 */
int buildin_getusers(struct script_state *st)
{
	int flag=conv_num(st,& (st->stack->stack_data[st->start+2]));
	struct block_list *bl=map_id2bl((flag&0x08)?st->oid:st->rid);
	int val=0;
	switch(flag&0x07){
	case 0: val=map[bl->m].users; break;
	case 1: val=map_getusers(); break;
	}
	push_val(st->stack,C_INT,val);
	return 0;
}
/*==========================================
 * Works like @WHO - displays all online users names in window
 *------------------------------------------
 */
int buildin_getusersname(struct script_state *st)
{
	struct map_session_data *pl_sd = NULL, **pl_allsd;
	int i=0,disp_num=1, users;

	pl_allsd = map_getallusers(&users);

	for (i=0;i<users;i++)
	{
		pl_sd = pl_allsd[i];
		if( !(battle_config.hide_GM_session && pc_isGM(pl_sd)) )
		{
			if((disp_num++)%10==0)
				clif_scriptnext(script_rid2sd(st),st->oid);
			clif_scriptmes(script_rid2sd(st),st->oid,pl_sd->status.name);
		}
	}
	return 0;
}
/*==========================================
 * マップ指定ユーザー数所得
 *------------------------------------------
 */
int buildin_getmapusers(struct script_state *st)
{
	char *str;
	int m;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	if( (m=map_mapname2mapid(str))< 0){
		push_val(st->stack,C_INT,-1);
		return 0;
	}
	push_val(st->stack,C_INT,map[m].users);
	return 0;
}
/*==========================================
 * エリア指定ユーザー数所得
 *------------------------------------------
 */
int buildin_getareausers_sub(struct block_list *bl,va_list ap)
{
	int *users=va_arg(ap,int *);
	(*users)++;
	return 0;
}
int buildin_getareausers(struct script_state *st)
{
	char *str;
	int m,x0,y0,x1,y1,users=0;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x0=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y0=conv_num(st,& (st->stack->stack_data[st->start+4]));
	x1=conv_num(st,& (st->stack->stack_data[st->start+5]));
	y1=conv_num(st,& (st->stack->stack_data[st->start+6]));
	if( (m=map_mapname2mapid(str))< 0){
		push_val(st->stack,C_INT,-1);
		return 0;
	}
	map_foreachinarea(buildin_getareausers_sub,
		m,x0,y0,x1,y1,BL_PC,&users);
	push_val(st->stack,C_INT,users);
	return 0;
}

/*==========================================
 * エリア指定ドロップアイテム数所得
 *------------------------------------------
 */
int buildin_getareadropitem_sub(struct block_list *bl,va_list ap)
{
	int item=va_arg(ap,int);
	int *amount=va_arg(ap,int *);
	struct flooritem_data *drop=(struct flooritem_data *)bl;

	if(drop->item_data.nameid==item)
		(*amount)+=drop->item_data.amount;

	return 0;
}
int buildin_getareadropitem(struct script_state *st)
{
	char *str;
	int m,x0,y0,x1,y1,item,amount=0;
	struct script_data *data;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x0=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y0=conv_num(st,& (st->stack->stack_data[st->start+4]));
	x1=conv_num(st,& (st->stack->stack_data[st->start+5]));
	y1=conv_num(st,& (st->stack->stack_data[st->start+6]));

	data=&(st->stack->stack_data[st->start+7]);
	get_val(st,data);
	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		item=512;
		if( item_data )
			item=item_data->nameid;
	}else
		item=conv_num(st,data);

	if( (m=map_mapname2mapid(str))< 0){
		push_val(st->stack,C_INT,-1);
		return 0;
	}
	map_foreachinarea(buildin_getareadropitem_sub,
		m,x0,y0,x1,y1,BL_ITEM,item,&amount);
	push_val(st->stack,C_INT,amount);
	return 0;
}
/*==========================================
 * NPCの有効化
 *------------------------------------------
 */
int buildin_enablenpc(struct script_state *st)
{
	char *str;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	npc_enable(str,1);
	return 0;
}
/*==========================================
 * NPCの無効化
 *------------------------------------------
 */
int buildin_disablenpc(struct script_state *st)
{
	char *str;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	npc_enable(str,0);
	return 0;
}

int buildin_enablearena(struct script_state *st)	// Added by RoVeRT
{
	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);
	struct chat_data *cd;


	if(nd==NULL || (cd=(struct chat_data *)map_id2bl(nd->chat_id))==NULL)
		return 0;

	npc_enable(nd->name,1);
	nd->arenaflag=1;

	if(cd->users>=cd->trigger && cd->npc_event[0])
		npc_timer_event(cd->npc_event);

	return 0;
}
int buildin_disablearena(struct script_state *st)	// Added by RoVeRT
{
	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);
	nd->arenaflag=0;

	return 0;
}
/*==========================================
 * 隠れているNPCの表示
 *------------------------------------------
 */
int buildin_hideoffnpc(struct script_state *st)
{
	char *str;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	npc_enable(str,2);
	return 0;
}
/*==========================================
 * NPCをハイディング
 *------------------------------------------
 */
int buildin_hideonnpc(struct script_state *st)
{
	char *str;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	npc_enable(str,4);
	return 0;
}
/*==========================================
 * 状態異常にかかる
 *------------------------------------------
 */
int buildin_sc_start(struct script_state *st)
{
	struct block_list *bl;
	int type,tick,val1,val4=0;
	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	tick=conv_num(st,& (st->stack->stack_data[st->start+3]));
	val1=conv_num(st,& (st->stack->stack_data[st->start+4]));
	if( st->end>st->start+5 ) //指定したキャラを状態異常にする
		bl = map_id2bl(conv_num(st,& (st->stack->stack_data[st->start+5])));
	else
		bl = map_id2bl(st->rid);

	if (potion_flag==1 && potion_target) {
		bl = map_id2bl(potion_target);
		tick/=2; //Thrown potions only last half.
		val4 = 1; //Mark that this was a thrown sc_effect
	}
	if (bl)
		sc_start4(bl,type,100,val1,0,0,val4,tick);
	return 0;
}

/*==========================================
 * 状態異常にかかる(確率指定)
 *------------------------------------------
 */
int buildin_sc_start2(struct script_state *st)
{
	struct block_list *bl;
	int type,tick,val1,val4=0,per;
	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	tick=conv_num(st,& (st->stack->stack_data[st->start+3]));
	val1=conv_num(st,& (st->stack->stack_data[st->start+4]));
	per=conv_num(st,& (st->stack->stack_data[st->start+5]));
	if( st->end>st->start+6 ) //指定したキャラを状態異常にする
		bl = map_id2bl(conv_num(st,& (st->stack->stack_data[st->start+6])));
	else
		bl = map_id2bl(st->rid);

	if (potion_flag==1 && potion_target) {
		bl = map_id2bl(potion_target);
		tick/=2;
		val4 = 1;
	}

	if(bl)
		status_change_start(bl,type,per,val1,0,0,val4,tick,0);
	return 0;
}

/*==========================================
 * Starts a SC_ change with the four values passed. [Skotlex]
 * Final optional argument is the ID of player to affect.
 * sc_start4 type, duration, val1, val2, val3, val4, <id>;
 *------------------------------------------
 */
int buildin_sc_start4(struct script_state *st)
{
	struct block_list *bl;
	int type,tick,val1,val2,val3,val4;
	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	tick=conv_num(st,& (st->stack->stack_data[st->start+3]));
	val1=conv_num(st,& (st->stack->stack_data[st->start+4]));
	val2=conv_num(st,& (st->stack->stack_data[st->start+5]));
	val3=conv_num(st,& (st->stack->stack_data[st->start+6]));
	val4=conv_num(st,& (st->stack->stack_data[st->start+7]));
	if( st->end>st->start+8 )
		bl = map_id2bl(conv_num(st,& (st->stack->stack_data[st->start+8])));
	else
		bl = map_id2bl(st->rid);

	if (potion_flag==1 && potion_target) {
		bl = map_id2bl(potion_target);
		tick/=2;
	}
	if (bl)
		sc_start4(bl,type,100,val1,val2,val3,val4,tick);
	return 0;
}

/*==========================================
 * 状態異常が直る
 *------------------------------------------
 */
int buildin_sc_end(struct script_state *st)
{
	struct block_list *bl;
	int type;
	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	bl = map_id2bl(st->rid);

	if (potion_flag==1 && potion_target)
		bl = map_id2bl(potion_target);

	if (bl)
		status_change_end(bl,type,-1);
	return 0;
}
/*==========================================
 * 状態異常耐性を計算した確率を返す
 *------------------------------------------
 */
int buildin_getscrate(struct script_state *st)
{
	struct block_list *bl;
	int sc_def=0,type,rate;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	rate=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if( st->end>st->start+4 ) //指定したキャラの耐性を計算する
		bl = map_id2bl(conv_num(st,& (st->stack->stack_data[st->start+6])));
	else
		bl = map_id2bl(st->rid);

	if (bl)
		sc_def = status_get_sc_def(bl,type);

	rate = rate*(10000-sc_def)/10000;
	push_val(st->stack,C_INT,rate<0?0:rate);

	return 0;

}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_debugmes(struct script_state *st)
{
	conv_str(st,& (st->stack->stack_data[st->start+2]));
	ShowDebug("script debug : %d %d : %s\n",st->rid,st->oid,st->stack->stack_data[st->start+2].u.str);
	return 0;
}

/*==========================================
 *捕獲アイテム使用
 *------------------------------------------
 */
int buildin_catchpet(struct script_state *st)
{
	int pet_id;
	struct map_session_data *sd;
	pet_id= conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	pet_catch_process1(sd,pet_id);
	return 0;
}

/*==========================================
 *携帯卵孵化機使用
 *------------------------------------------
 */
int buildin_birthpet(struct script_state *st)
{
	struct map_session_data *sd;
	sd=script_rid2sd(st);
	clif_sendegg(sd);
	return 0;
}

/*==========================================
 * Added - AppleGirl For Advanced Classes, (Updated for Cleaner Script Purposes)
 *------------------------------------------
 */
int buildin_resetlvl(struct script_state *st)
{
	struct map_session_data *sd;

	int type=conv_num(st,& (st->stack->stack_data[st->start+2]));

	sd=script_rid2sd(st);
	pc_resetlvl(sd,type);
	return 0;
}
/*==========================================
 * ステータスリセット
 *------------------------------------------
 */
int buildin_resetstatus(struct script_state *st)
{
	struct map_session_data *sd;
	sd=script_rid2sd(st);
	pc_resetstate(sd);
	return 0;
}

/*==========================================
 * script command resetskill
 *------------------------------------------
 */
int buildin_resetskill(struct script_state *st)
{
	struct map_session_data *sd;
	sd=script_rid2sd(st);
	pc_resetskill(sd,1);
	return 0;
}

/*==========================================
 * Counts total amount of skill points.
 *------------------------------------------
 */
int buildin_skillpointcount(struct script_state *st)
{
	struct map_session_data *sd;
	sd=script_rid2sd(st);
	push_val(st->stack,C_INT,sd->status.skill_point + pc_resetskill(sd,2));
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_changebase(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	int vclass;

	if( st->end>st->start+3 )
		sd=map_id2sd(conv_num(st,& (st->stack->stack_data[st->start+3])));
	else
	sd=script_rid2sd(st);

	if(sd == NULL)
		return 0;

	vclass = conv_num(st,& (st->stack->stack_data[st->start+2]));
	if(vclass == JOB_WEDDING)
	{
		if (!battle_config.wedding_modifydisplay ||	//Do not show the wedding sprites
			sd->class_&JOBL_BABY //Baby classes screw up when showing wedding sprites. [Skotlex]
			)
		return 0;
	}

	if(!sd->disguise && vclass != sd->vd.class_) {
		status_set_viewdata(&sd->bl, vclass);
		//Updated client view. Base, Weapon and Cloth Colors.
		clif_changelook(&sd->bl,LOOK_BASE,sd->vd.class_);
		clif_changelook(&sd->bl,LOOK_WEAPON,sd->status.weapon);
		if (sd->vd.cloth_color)
			clif_changelook(&sd->bl,LOOK_CLOTHES_COLOR,sd->vd.cloth_color);
	}

	return 0;
}

/*==========================================
 * 性別変換
 *------------------------------------------
 */
int buildin_changesex(struct script_state *st) {
	struct map_session_data *sd = NULL;
	sd = script_rid2sd(st);

	if (sd->status.sex == 0) {
		sd->status.sex = 1;
		sd->sex = 1;
		if ((sd->class_&MAPID_UPPERMASK) == MAPID_BARDDANCER)
			sd->status.class_ -= 1;
	} else if (sd->status.sex == 1) {
		sd->status.sex = 0;
		sd->sex = 0;
		if ((sd->class_&MAPID_UPPERMASK) == MAPID_BARDDANCER)
			sd->status.class_ += 1;
	}
	chrif_char_ask_name(-1, sd->status.name, 5, 0, 0, 0, 0, 0, 0); // type: 5 - changesex
	chrif_save(sd,0);
	return 0;
}

/*==========================================
 * npcチャット作成
 *------------------------------------------
 */
int buildin_waitingroom(struct script_state *st)
{
	char *name,*ev="";
	int limit, trigger = 0,pub=1;
	name=conv_str(st,& (st->stack->stack_data[st->start+2]));
	limit= conv_num(st,& (st->stack->stack_data[st->start+3]));
	if(limit==0)
		pub=3;

	if( (st->end > st->start+5) ){
		struct script_data* data=&(st->stack->stack_data[st->start+5]);
		get_val(st,data);
		if(data->type==C_INT){
			// 新Athena仕様(旧Athena仕様と互換性あり)
			ev=conv_str(st,& (st->stack->stack_data[st->start+4]));
			trigger=conv_num(st,& (st->stack->stack_data[st->start+5]));
		}else{
			// eathena仕様
			trigger=conv_num(st,& (st->stack->stack_data[st->start+4]));
			ev=conv_str(st,& (st->stack->stack_data[st->start+5]));
		}
	}else{
		// 旧Athena仕様
		if( st->end > st->start+4 )
			ev=conv_str(st,& (st->stack->stack_data[st->start+4]));
	}
	chat_createnpcchat( (struct npc_data *)map_id2bl(st->oid),
		limit,pub,trigger,name,(int)strlen(name)+1,ev);
	return 0;
}
/*==========================================
 * Works like 'announce' but outputs in the common chat window
 *------------------------------------------
 */
int buildin_globalmes(struct script_state *st)
{
	struct block_list *bl = map_id2bl(st->oid);
	struct npc_data *nd = (struct npc_data *)bl;
	char *name=NULL,*mes;

	mes=conv_str(st,& (st->stack->stack_data[st->start+2]));	// メッセージの取得
	if(mes==NULL) return 0;

	if(st->end>st->start+3){	// NPC名の取得(123#456)
		name=conv_str(st,& (st->stack->stack_data[st->start+3]));
	} else {
		name=nd->name;
	}

	npc_globalmessage(name,mes);	// グローバルメッセージ送信

	return 0;
}
/*==========================================
 * npcチャット削除
 *------------------------------------------
 */
int buildin_delwaitingroom(struct script_state *st)
{
	struct npc_data *nd;
	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);
	chat_deletenpcchat(nd);
	return 0;
}
/*==========================================
 * npcチャット全員蹴り出す
 *------------------------------------------
 */
int buildin_waitingroomkickall(struct script_state *st)
{
	struct npc_data *nd;
	struct chat_data *cd;

	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	if(nd==NULL || (cd=(struct chat_data *)map_id2bl(nd->chat_id))==NULL )
		return 0;
	chat_npckickall(cd);
	return 0;
}

/*==========================================
 * npcチャットイベント有効化
 *------------------------------------------
 */
int buildin_enablewaitingroomevent(struct script_state *st)
{
	struct npc_data *nd;
	struct chat_data *cd;

	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	if(nd==NULL || (cd=(struct chat_data *)map_id2bl(nd->chat_id))==NULL )
		return 0;
	chat_enableevent(cd);
	return 0;
}

/*==========================================
 * npcチャットイベント無効化
 *------------------------------------------
 */
int buildin_disablewaitingroomevent(struct script_state *st)
{
	struct npc_data *nd;
	struct chat_data *cd;

	if( st->end > st->start+2 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+2])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	if(nd==NULL || (cd=(struct chat_data *)map_id2bl(nd->chat_id))==NULL )
		return 0;
	chat_disableevent(cd);
	return 0;
}
/*==========================================
 * npcチャット状態所得
 *------------------------------------------
 */
int buildin_getwaitingroomstate(struct script_state *st)
{
	struct npc_data *nd;
	struct chat_data *cd;
	int val=0,type;
	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if( st->end > st->start+3 )
		nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+3])));
	else
		nd=(struct npc_data *)map_id2bl(st->oid);

	if(nd==NULL || (cd=(struct chat_data *)map_id2bl(nd->chat_id))==NULL ){
		push_val(st->stack,C_INT,-1);
		return 0;
	}

	switch(type){
	case 0: val=cd->users; break;
	case 1: val=cd->limit; break;
	case 2: val=cd->trigger&0x7f; break;
	case 3: val=((cd->trigger&0x80)>0); break;
	case 32: val=(cd->users >= cd->limit); break;
	case 33: val=(cd->users >= cd->trigger); break;

	case 4:
		push_str(st->stack,C_CONSTSTR,(unsigned char *) cd->title);
		return 0;
	case 5:
		push_str(st->stack,C_CONSTSTR,(unsigned char *) cd->pass);
		return 0;
	case 16:
		push_str(st->stack,C_CONSTSTR,(unsigned char *) cd->npc_event);
		return 0;
	}
	push_val(st->stack,C_INT,val);
	return 0;
}

/*==========================================
 * チャットメンバー(規定人数)ワープ
 *------------------------------------------
 */
int buildin_warpwaitingpc(struct script_state *st)
{
	int x,y,i,n;
	char *str;
	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);
	struct chat_data *cd;

	if(nd==NULL || (cd=(struct chat_data *)map_id2bl(nd->chat_id))==NULL )
		return 0;

	n=cd->trigger&0x7f;
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));

	if( st->end > st->start+5 )
		n=conv_num(st,& (st->stack->stack_data[st->start+5]));

	for(i=0;i<n;i++){
		struct map_session_data *sd=cd->usersd[0];	// リスト先頭のPCを次々に。

		mapreg_setreg(add_str((unsigned char *) "$@warpwaitingpc")+(i<<24),sd->bl.id);

		if(strcmp(str,"Random")==0)
			pc_randomwarp(sd,3);
		else if(strcmp(str,"SavePoint")==0){
			if(map[sd->bl.m].flag.noteleport)	// テレポ禁止
				return 0;

			pc_setpos(sd,sd->status.save_point.map,
				sd->status.save_point.x,sd->status.save_point.y,3);
		}else
			pc_setpos(sd,mapindex_name2id(str),x,y,0);
	}
	mapreg_setreg(add_str((unsigned char *) "$@warpwaitingpcnum"),n);
	return 0;
}
/*==========================================
 * RIDのアタッチ
 *------------------------------------------
 */
int buildin_attachrid(struct script_state *st)
{
	st->rid=conv_num(st,& (st->stack->stack_data[st->start+2]));
	push_val(st->stack,C_INT, (map_id2sd(st->rid)!=NULL));
	return 0;
}
/*==========================================
 * RIDのデタッチ
 *------------------------------------------
 */
int buildin_detachrid(struct script_state *st)
{
	st->rid=0;
	return 0;
}
/*==========================================
 * 存在チェック
 *------------------------------------------
 */
int buildin_isloggedin(struct script_state *st)
{
	push_val(st->stack,C_INT, map_id2sd(
		conv_num(st,& (st->stack->stack_data[st->start+2])) )!=NULL );
	return 0;
}


/*==========================================
 *
 *------------------------------------------
 */
enum {  MF_NOMEMO,MF_NOTELEPORT,MF_NOSAVE,MF_NOBRANCH,MF_NOPENALTY,MF_NOZENYPENALTY,
	MF_PVP,MF_PVP_NOPARTY,MF_PVP_NOGUILD,MF_GVG,MF_GVG_NOPARTY,MF_NOTRADE,MF_NOSKILL,
	MF_NOWARP,MF_FREE,MF_NOICEWALL,MF_SNOW,MF_FOG,MF_SAKURA,MF_LEAVES,MF_RAIN,
	MF_INDOORS,MF_NOGO,MF_CLOUDS,MF_CLOUDS2,MF_FIREWORKS,MF_GVG_CASTLE,MF_GVG_DUNGEON,MF_NIGHTENABLED,
	MF_NOBASEEXP, MF_NOJOBEXP, MF_NOMOBLOOT, MF_NOMVPLOOT, MF_NORETURN, MF_NOWARPTO, MF_NIGHTMAREDROP,
	MF_RESTRICTED, MF_NOCOMMAND, MF_NODROP };

int buildin_setmapflagnosave(struct script_state *st)
{
	int m,x,y;
	unsigned short mapindex;
	char *str,*str2;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	str2=conv_str(st,& (st->stack->stack_data[st->start+3]));
	x=conv_num(st,& (st->stack->stack_data[st->start+4]));
	y=conv_num(st,& (st->stack->stack_data[st->start+5]));
	m = map_mapname2mapid(str);
	mapindex = mapindex_name2id(str2);

	if(m >= 0 && mapindex) {
		map[m].flag.nosave=1;
		map[m].save.map=mapindex;
		map[m].save.x=x;
		map[m].save.y=y;
	}

	return 0;
}

int buildin_setmapflag(struct script_state *st)
{
	int m,i;
	char *str;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	i=conv_num(st,& (st->stack->stack_data[st->start+3]));
	m = map_mapname2mapid(str);
	if(m >= 0) {
		switch(i) {
			case MF_NOMEMO:
				map[m].flag.nomemo=1;
				break;
			case MF_NOTELEPORT:
				map[m].flag.noteleport=1;
				break;
			case MF_NOBRANCH:
				map[m].flag.nobranch=1;
				break;
			case MF_NOPENALTY:
				map[m].flag.nopenalty=1;
				break;
			case MF_NOZENYPENALTY:
				map[m].flag.nozenypenalty=1;
				break;
			case MF_PVP:
				map[m].flag.pvp=1;
				break;
			case MF_PVP_NOPARTY:
				map[m].flag.pvp_noparty=1;
				break;
			case MF_PVP_NOGUILD:
				map[m].flag.pvp_noguild=1;
				break;
			case MF_GVG:
				map[m].flag.gvg=1;
				break;
			case MF_GVG_NOPARTY:
				map[m].flag.gvg_noparty=1;
				break;
			case MF_GVG_DUNGEON:
				map[m].flag.gvg_dungeon=1;
				break;
			case MF_GVG_CASTLE:
				map[m].flag.gvg_castle=1;
				break;
			case MF_NOTRADE:
				map[m].flag.notrade=1;
				break;
			case MF_NODROP:
				map[m].flag.nodrop=1;
				break;
			case MF_NOSKILL:
				map[m].flag.noskill=1;
				break;
			case MF_NOWARP:
				map[m].flag.nowarp=1;
				break;
			case MF_NOICEWALL: // [Valaris]
				map[m].flag.noicewall=1;
				break;
			case MF_SNOW: // [Valaris]
				map[m].flag.snow=1;
				break;
			case MF_CLOUDS:
				map[m].flag.clouds=1;
				break;
			case MF_CLOUDS2: // [Valaris]
				map[m].flag.clouds2=1;
				break;
			case MF_FOG: // [Valaris]
				map[m].flag.fog=1;
				break;
			case MF_FIREWORKS:
				map[m].flag.fireworks=1;
				break;
			case MF_SAKURA: // [Valaris]
				map[m].flag.sakura=1;
				break;
			case MF_LEAVES: // [Valaris]
				map[m].flag.leaves=1;
				break;
			case MF_RAIN: // [Valaris]
				map[m].flag.rain=1;
				break;
			case MF_INDOORS: // celest
				map[m].flag.indoors=1;
				break;
			case MF_NIGHTENABLED:
				map[m].flag.nightenabled=1;
				break;
			case MF_NOGO: // celest
				map[m].flag.nogo=1;
				break;
			case MF_NOBASEEXP:
				map[m].flag.nobaseexp=1;
				break;
			case MF_NOJOBEXP:
				map[m].flag.nojobexp=1;
				break;
			case MF_NOMOBLOOT:
				map[m].flag.nomobloot=1;
				break;
			case MF_NOMVPLOOT:
				map[m].flag.nomvploot=1;
				break;
			case MF_NORETURN:
				map[m].flag.noreturn=1;
				break;
			case MF_NOWARPTO:
				map[m].flag.nowarpto=1;
				break;
			case MF_NIGHTMAREDROP:
				map[m].flag.pvp_nightmaredrop=1;
				break;
			case MF_RESTRICTED:
				map[m].flag.restricted=1;
				break;
			case MF_NOCOMMAND:
				map[m].flag.nocommand=1;
				break;
		}
	}

	return 0;
}

int buildin_removemapflag(struct script_state *st)
{
	int m,i;
	char *str;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	i=conv_num(st,& (st->stack->stack_data[st->start+3]));
	m = map_mapname2mapid(str);
	if(m >= 0) {
		switch(i) {
			case MF_NOMEMO:
				map[m].flag.nomemo=0;
				break;
			case MF_NOTELEPORT:
				map[m].flag.noteleport=0;
				break;
			case MF_NOSAVE:
				map[m].flag.nosave=0;
				break;
			case MF_NOBRANCH:
				map[m].flag.nobranch=0;
				break;
			case MF_NOPENALTY:
				map[m].flag.nopenalty=0;
				break;
			case MF_PVP:
				map[m].flag.pvp=0;
				break;
			case MF_PVP_NOPARTY:
				map[m].flag.pvp_noparty=0;
				break;
			case MF_PVP_NOGUILD:
				map[m].flag.pvp_noguild=0;
				break;
			case MF_GVG:
				map[m].flag.gvg=0;
				break;
			case MF_GVG_NOPARTY:
				map[m].flag.gvg_noparty=0;
				break;
			case MF_GVG_DUNGEON:
				map[m].flag.gvg_dungeon=0;
				break;
			case MF_GVG_CASTLE:
				map[m].flag.gvg_castle=0;
				break;
			case MF_NOZENYPENALTY:
				map[m].flag.nozenypenalty=0;
				break;
			case MF_NOTRADE:
				map[m].flag.notrade=0;
				break;
			case MF_NODROP:
				map[m].flag.nodrop=0;
				break;
			case MF_NOSKILL:
				map[m].flag.noskill=0;
				break;
			case MF_NOWARP:
				map[m].flag.nowarp=0;
				break;
			case MF_NOICEWALL: // [Valaris]
				map[m].flag.noicewall=0;
				break;
			case MF_SNOW: // [Valaris]
				map[m].flag.snow=0;
				break;
			case MF_CLOUDS:
				map[m].flag.clouds=0;
				break;
			case MF_CLOUDS2: // [Valaris]
				map[m].flag.clouds2=0;
				break;
			case MF_FOG: // [Valaris]
				map[m].flag.fog=0;
				break;
			case MF_FIREWORKS:
				map[m].flag.fireworks=0;
				break;
			case MF_SAKURA: // [Valaris]
				map[m].flag.sakura=0;
				break;
			case MF_LEAVES: // [Valaris]
				map[m].flag.leaves=0;
				break;
			case MF_RAIN: // [Valaris]
				map[m].flag.rain=0;
				break;
			case MF_INDOORS: // celest
				map[m].flag.indoors=0;
				break;
			case MF_NIGHTENABLED:
				map[m].flag.nightenabled=0;
				break;
			case MF_NOGO: // celest
				map[m].flag.nogo=0;
				break;
			case MF_NOBASEEXP:
				map[m].flag.nobaseexp=0;
				break;
			case MF_NOJOBEXP:
				map[m].flag.nojobexp=0;
				break;
			case MF_NOMOBLOOT:
				map[m].flag.nomobloot=0;
				break;
			case MF_NOMVPLOOT:
				map[m].flag.nomvploot=0;
				break;
			case MF_NORETURN:
				map[m].flag.noreturn=0;
				break;
			case MF_NOWARPTO:
				map[m].flag.nowarpto=0;
				break;
			case MF_NIGHTMAREDROP:
				map[m].flag.pvp_nightmaredrop=0;
				break;
			case MF_RESTRICTED:
				map[m].flag.restricted=0;
				break;
			case MF_NOCOMMAND:
				map[m].flag.nocommand=0;
				break;
		}
	}

	return 0;
}

int buildin_pvpon(struct script_state *st)
{
	int m,i,users;
	char *str;
	struct map_session_data *pl_sd=NULL, **pl_allsd;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	m = map_mapname2mapid(str);
	if(m >= 0 && !map[m].flag.pvp) {
		map[m].flag.pvp = 1;
		clif_send0199(m,1);

		if(battle_config.pk_mode) // disable ranking functions if pk_mode is on [Valaris]
			return 0;

		pl_allsd = map_getallusers(&users);

		for(i=0;i<users;i++)
		{
			if ((pl_sd = pl_allsd[i]) && m == pl_sd->bl.m && pl_sd->pvp_timer == -1)
			{
				pl_sd->pvp_timer=add_timer(gettick()+200,pc_calc_pvprank_timer,pl_sd->bl.id,0);
				pl_sd->pvp_rank=0;
				pl_sd->pvp_lastusers=0;
				pl_sd->pvp_point=5;
				pl_sd->pvp_won = 0;
				pl_sd->pvp_lost = 0;
			}
		}
	}
	return 0;
}

int buildin_pvpoff(struct script_state *st)
{
	int m,i,users;
	char *str;
	struct map_session_data *pl_sd=NULL, **pl_allsd;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	m = map_mapname2mapid(str);
	if(m >= 0 && map[m].flag.pvp) { //fixed Lupus
		map[m].flag.pvp = 0;
		clif_send0199(m,0);

		if(battle_config.pk_mode) // disable ranking options if pk_mode is on [Valaris]
			return 0;

		pl_allsd = map_getallusers(&users);

		for(i=0;i<users;i++)
		{
			if((pl_sd=pl_allsd[i]) && m == pl_sd->bl.m)
			{
				clif_pvpset(pl_sd,0,0,2);
				if(pl_sd->pvp_timer != -1) {
					delete_timer(pl_sd->pvp_timer,pc_calc_pvprank_timer);
					pl_sd->pvp_timer = -1;
				}
			}
		}
	}

	return 0;
}

int buildin_gvgon(struct script_state *st)
{
	int m;
	char *str;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	m = map_mapname2mapid(str);
	if(m >= 0 && !map[m].flag.gvg) {
		map[m].flag.gvg = 1;
		clif_send0199(m,3);
	}

	return 0;
}
int buildin_gvgoff(struct script_state *st)
{
	int m;
	char *str;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	m = map_mapname2mapid(str);
	if(m >= 0 && map[m].flag.gvg) {
		map[m].flag.gvg = 0;
		clif_send0199(m,0);
	}

	return 0;
}
/*==========================================
 *	Shows an emoticon on top of the player/npc
 *	emotion emotion#, <target: 0 - NPC, 1 - PC>
 *------------------------------------------
 */
//Optional second parameter added by [Skotlex]
int buildin_emotion(struct script_state *st)
{
	int type;
	int player=0;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if(type < 0 || type > 100)
		return 0;

	if( st->end>st->start+3 )
		player=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if (player) {
		struct map_session_data *sd = script_rid2sd(st);
		if (sd)
			clif_emotion(&sd->bl,type);
	} else
		clif_emotion(map_id2bl(st->oid),type);
	return 0;
}

int buildin_maprespawnguildid_sub(struct block_list *bl,va_list ap)
{
	int g_id=va_arg(ap,int);
	int flag=va_arg(ap,int);
	struct map_session_data *sd=NULL;
	struct mob_data *md=NULL;

	if(bl->type == BL_PC)
		sd=(struct map_session_data*)bl;
	if(bl->type == BL_MOB)
		md=(struct mob_data *)bl;

	if(sd){
		if((sd->status.guild_id == g_id) && (flag&1))
			pc_setpos(sd,sd->status.save_point.map,sd->status.save_point.x,sd->status.save_point.y,3);
		else if((sd->status.guild_id != g_id) && (flag&2))
			pc_setpos(sd,sd->status.save_point.map,sd->status.save_point.x,sd->status.save_point.y,3);
		else if (sd->status.guild_id == 0)	// Warp out players not in guild [Valaris]
			pc_setpos(sd,sd->status.save_point.map,sd->status.save_point.x,sd->status.save_point.y,3);	// end addition [Valaris]
	}
	if(md && flag&4){
		if(!md->guardian_data && md->class_ != MOBID_EMPERIUM)
			unit_remove_map(bl,1);
	}
	return 0;
}
int buildin_maprespawnguildid(struct script_state *st)
{
	char *mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	int g_id=conv_num(st,& (st->stack->stack_data[st->start+3]));
	int flag=conv_num(st,& (st->stack->stack_data[st->start+4]));

	int m=map_mapname2mapid(mapname);

	if(m) map_foreachinmap(buildin_maprespawnguildid_sub,m,BL_CHAR,g_id,flag);
	return 0;
}

int buildin_agitstart(struct script_state *st)
{
	if(agit_flag==1) return 0;      // Agit already Start.
	agit_flag=1;
	guild_agit_start();
	return 0;
}

int buildin_agitend(struct script_state *st)
{
	if(agit_flag==0) return 0;      // Agit already End.
	agit_flag=0;
	guild_agit_end();
	return 0;
}
/*==========================================
 * agitcheck 1;    // choice script
 * if(@agit_flag == 1) goto agit;
 * if(agitcheck(0) == 1) goto agit;
 *------------------------------------------
 */
int buildin_agitcheck(struct script_state *st)
{
	struct map_session_data *sd;
	int cond;

	cond=conv_num(st,& (st->stack->stack_data[st->start+2]));

	if(cond == 0) {
		if (agit_flag==1) push_val(st->stack,C_INT,1);
		if (agit_flag==0) push_val(st->stack,C_INT,0);
	} else {
		sd=script_rid2sd(st);
		if (agit_flag==1) pc_setreg(sd,add_str((unsigned char *) "@agit_flag"),1);
		if (agit_flag==0) pc_setreg(sd,add_str((unsigned char *) "@agit_flag"),0);
	}
	return 0;
}
int buildin_flagemblem(struct script_state *st)
{
	int g_id=conv_num(st,& (st->stack->stack_data[st->start+2]));

	if(g_id < 0) return 0;

//	printf("Script.c: [FlagEmblem] GuildID=%d, Emblem=%d.\n", g->guild_id, g->emblem_id);
	((struct npc_data *)map_id2bl(st->oid))->u.scr.guild_id = g_id;
	return 0;
}

int buildin_getcastlename(struct script_state *st)
{
	char *mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	struct guild_castle *gc;
	int i;
	char *buf=NULL;
	for(i=0;i<MAX_GUILDCASTLE;i++){
		if( (gc=guild_castle_search(i)) != NULL ){
			if(strcmp(mapname,gc->map_name)==0){
				buf=(char *)aCallocA(NAME_LENGTH,sizeof(char));
				memcpy(buf, gc->castle_name, NAME_LENGTH-1);
				break;
			}
		}
	}
	if(buf)
	push_str(st->stack,C_STR,(unsigned char *) buf);
	else
		push_str(st->stack,C_CONSTSTR,(unsigned char *) "");
	return 0;
}

int buildin_getcastledata(struct script_state *st)
{
	char *mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	int index=conv_num(st,& (st->stack->stack_data[st->start+3]));
	char *event=NULL;
	struct guild_castle *gc;
	int i,j;

	if( st->end>st->start+4 && index==0){
		for(i=0,j=-1;i<MAX_GUILDCASTLE;i++)
			if( (gc=guild_castle_search(i)) != NULL &&
				strcmp(mapname,gc->map_name)==0 )
				j=i;
		if(j>=0){
			event=conv_str(st,& (st->stack->stack_data[st->start+4]));
			guild_addcastleinfoevent(j,17,event);
		}
	}

	for(i=0;i<MAX_GUILDCASTLE;i++){
		if( (gc=guild_castle_search(i)) != NULL ){
			if(strcmp(mapname,gc->map_name)==0){
				switch(index){
				case 0: for(j=1;j<26;j++) guild_castledataload(gc->castle_id,j); break;  // Initialize[AgitInit]
				case 1: push_val(st->stack,C_INT,gc->guild_id); break;
				case 2: push_val(st->stack,C_INT,gc->economy); break;
				case 3: push_val(st->stack,C_INT,gc->defense); break;
				case 4: push_val(st->stack,C_INT,gc->triggerE); break;
				case 5: push_val(st->stack,C_INT,gc->triggerD); break;
				case 6: push_val(st->stack,C_INT,gc->nextTime); break;
				case 7: push_val(st->stack,C_INT,gc->payTime); break;
				case 8: push_val(st->stack,C_INT,gc->createTime); break;
				case 9: push_val(st->stack,C_INT,gc->visibleC); break;
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16:
				case 17:
					push_val(st->stack,C_INT,gc->guardian[index-10].visible); break;
				case 18:
				case 19:
				case 20:
				case 21:
				case 22:
				case 23:
				case 24:
				case 25:
					push_val(st->stack,C_INT,gc->guardian[index-18].hp); break;
				default:
					push_val(st->stack,C_INT,0); break;
				}
				return 0;
			}
		}
	}
	push_val(st->stack,C_INT,0);
	return 0;
}

int buildin_setcastledata(struct script_state *st)
{
	char *mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	int index=conv_num(st,& (st->stack->stack_data[st->start+3]));
	int value=conv_num(st,& (st->stack->stack_data[st->start+4]));
	struct guild_castle *gc;
	int i;

	for(i=0;i<MAX_GUILDCASTLE;i++){
		if( (gc=guild_castle_search(i)) != NULL ){
			if(strcmp(mapname,gc->map_name)==0){
				// Save Data byself First
				switch(index){
				case 1: gc->guild_id = value; break;
				case 2: gc->economy = value; break;
				case 3: gc->defense = value; break;
				case 4: gc->triggerE = value; break;
				case 5: gc->triggerD = value; break;
				case 6: gc->nextTime = value; break;
				case 7: gc->payTime = value; break;
				case 8: gc->createTime = value; break;
				case 9: gc->visibleC = value; break;
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16:
				case 17:
					gc->guardian[index-10].visible = value; break;
				case 18:
				case 19:
				case 20:
				case 21:
				case 22:
				case 23:
				case 24:
				case 25:
					gc->guardian[index-18].hp = value; break;
				default: return 0;
				}
				guild_castledatasave(gc->castle_id,index,value);
				return 0;
			}
		}
	}
	return 0;
}

/* =====================================================================
 * ギルド情報を要求する
 * ---------------------------------------------------------------------
 */
int buildin_requestguildinfo(struct script_state *st)
{
	int guild_id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	char *event=NULL;

	if( st->end>st->start+3 )
		event=conv_str(st,& (st->stack->stack_data[st->start+3]));

	if(guild_id>0)
		guild_npc_request_info(guild_id,event);
	return 0;
}

/* =====================================================================
 * カードの数を得る
 * ---------------------------------------------------------------------
 */
int buildin_getequipcardcnt(struct script_state *st)
{
	int i,num;
	struct map_session_data *sd;
	int c=MAX_SLOTS;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(sd->status.inventory[i].card[0] == 0x00ff){ // 製造武器はカードなし
		push_val(st->stack,C_INT,0);
		return 0;
	}
	do{
		if( (sd->status.inventory[i].card[c-1] > 4000 &&
			sd->status.inventory[i].card[c-1] < 5000) ||
			itemdb_type(sd->status.inventory[i].card[c-1]) == 6){	// [Celest]
			push_val(st->stack,C_INT,(c));
			return 0;
		}
	}while(c--);
	push_val(st->stack,C_INT,0);
	return 0;
}

/* ================================================================
 * カード取り外し成功
 * ----------------------------------------------------------------
 */
int buildin_successremovecards(struct script_state *st)
{
	int i,j,num,cardflag=0,flag;
	struct map_session_data *sd;
	struct item item_tmp;
	int c=MAX_SLOTS;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(sd->status.inventory[i].card[0]==0x00ff){ // 製造武器は処理しない
		return 0;
	}
	do{
		if( (sd->status.inventory[i].card[c-1] > 4000 &&
			sd->status.inventory[i].card[c-1] < 5000) ||
			itemdb_type(sd->status.inventory[i].card[c-1]) == 6){	// [Celest]

			cardflag = 1;
			item_tmp.id=0,item_tmp.nameid=sd->status.inventory[i].card[c-1];
			item_tmp.equip=0,item_tmp.identify=1,item_tmp.refine=0;
			item_tmp.attribute=0;
			for (j = 0; j < MAX_SLOTS; j++)
				item_tmp.card[j]=0;

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, item_tmp.nameid, 1, NULL);
			}
			//Logs

			if((flag=pc_additem(sd,&item_tmp,1))){	// 持てないならドロップ
				clif_additem(sd,0,0,flag);
				map_addflooritem(&item_tmp,1,sd->bl.m,sd->bl.x,sd->bl.y,NULL,NULL,NULL,0);
			}
		}
	}while(c--);

	if(cardflag == 1){	// カードを取り除いたアイテム所得
		flag=0;
		item_tmp.id=0,item_tmp.nameid=sd->status.inventory[i].nameid;
		item_tmp.equip=0,item_tmp.identify=1,item_tmp.refine=sd->status.inventory[i].refine;
		item_tmp.attribute=sd->status.inventory[i].attribute;

		//Logs items, got from (N)PC scripts [Lupus]
		if(log_config.pick > 0 ) {
			log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -1, &sd->status.inventory[i]);
		}
		//Logs

		for (j = 0; j < MAX_SLOTS; j++)
			item_tmp.card[j]=0;
		pc_delitem(sd,i,1,0);

		//Logs items, got from (N)PC scripts [Lupus]
		if(log_config.pick > 0 ) {
			log_pick(sd, "N", 0, item_tmp.nameid, 1, &item_tmp);
		}
		//Logs

		if((flag=pc_additem(sd,&item_tmp,1))){	// もてないならドロップ
			clif_additem(sd,0,0,flag);
			map_addflooritem(&item_tmp,1,sd->bl.m,sd->bl.x,sd->bl.y,NULL,NULL,NULL,0);
		}
		clif_misceffect(&sd->bl,3);
		return 0;
	}
	return 0;
}

/* ================================================================
 * カード取り外し失敗 slot,type
 * type=0: 両方損失、1:カード損失、2:武具損失、3:損失無し
 * ----------------------------------------------------------------
 */
int buildin_failedremovecards(struct script_state *st)
{
	int i,j,num,cardflag=0,flag,typefail;
	struct map_session_data *sd;
	struct item item_tmp;
	int c=MAX_SLOTS;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	typefail=conv_num(st,& (st->stack->stack_data[st->start+3]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(sd->status.inventory[i].card[0]==0x00ff){ // 製造武器は処理しない
		return 0;
	}
	do{
		if( (sd->status.inventory[i].card[c-1] > 4000 &&
			sd->status.inventory[i].card[c-1] < 5000) ||
			itemdb_type(sd->status.inventory[i].card[c-1]) == 6){	// [Celest]

			cardflag = 1;

			if(typefail == 2){ // 武具のみ損失なら、カードは受け取らせる
				item_tmp.id=0,item_tmp.nameid=sd->status.inventory[i].card[c-1];
				item_tmp.equip=0,item_tmp.identify=1,item_tmp.refine=0;
				item_tmp.attribute=0;
				for (j = 0; j < MAX_SLOTS; j++)
					item_tmp.card[j]=0;

				//Logs items, got from (N)PC scripts [Lupus]
				if(log_config.pick > 0 ) {
					log_pick(sd, "N", 0, item_tmp.nameid, 1, NULL);
				}
				//Logs

				if((flag=pc_additem(sd,&item_tmp,1))){
					clif_additem(sd,0,0,flag);
					map_addflooritem(&item_tmp,1,sd->bl.m,sd->bl.x,sd->bl.y,NULL,NULL,NULL,0);
				}
			}
		}
	}while(c--);

	if(cardflag == 1){

		if(typefail == 0 || typefail == 2){	// 武具損失
			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -1, &sd->status.inventory[i]);
			}
			//Logs

			pc_delitem(sd,i,1,0);
			clif_misceffect(&sd->bl,2);
			return 0;
		}
		if(typefail == 1){	// カードのみ損失（武具を返す）
			flag=0;
			item_tmp.id=0,item_tmp.nameid=sd->status.inventory[i].nameid;
			item_tmp.equip=0,item_tmp.identify=1,item_tmp.refine=sd->status.inventory[i].refine;
			item_tmp.attribute=sd->status.inventory[i].attribute;

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -1, &sd->status.inventory[i]);
			}
			//Logs

			for (j = 0; j < MAX_SLOTS; j++)
				item_tmp.card[j]=0;
			pc_delitem(sd,i,1,0);

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, item_tmp.nameid, 1, &item_tmp);
			}
			//Logs

			if((flag=pc_additem(sd,&item_tmp,1))){
				clif_additem(sd,0,0,flag);
				map_addflooritem(&item_tmp,1,sd->bl.m,sd->bl.x,sd->bl.y,NULL,NULL,NULL,0);
			}
		}
		clif_misceffect(&sd->bl,2);
		return 0;
	}
	return 0;
}

int buildin_mapwarp(struct script_state *st)	// Added by RoVeRT
{
	int x,y,m;
	char *str;
	char *mapname;
	unsigned int index;
	mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	str=conv_str(st,& (st->stack->stack_data[st->start+3]));
	x=conv_num(st,& (st->stack->stack_data[st->start+4]));
	y=conv_num(st,& (st->stack->stack_data[st->start+5]));

	if( (m=map_mapname2mapid(mapname))< 0)
		return 0;

	if(!(index=mapindex_name2id(str)))
		return 0;
	map_foreachinmap(buildin_areawarp_sub,
		m,BL_PC,index,x,y);
	return 0;
}

int buildin_cmdothernpc(struct script_state *st)	// Added by RoVeRT
{
	char *npc,*command;

	npc=conv_str(st,& (st->stack->stack_data[st->start+2]));
	command=conv_str(st,& (st->stack->stack_data[st->start+3]));

	npc_command(map_id2sd(st->rid),npc,command);
	return 0;
}

int buildin_inittimer(struct script_state *st)	// Added by RoVeRT
{
//	struct npc_data *nd=(struct npc_data*)map_id2bl(st->oid);
//	nd->lastaction=nd->timer=gettick();

	npc_do_ontimer(st->oid, 1);

	return 0;
}

int buildin_stoptimer(struct script_state *st)	// Added by RoVeRT
{
//	struct npc_data *nd=(struct npc_data*)map_id2bl(st->oid);
//	nd->lastaction=nd->timer=-1;

	npc_do_ontimer(st->oid, 0);

	return 0;
}

int buildin_mobcount_sub(struct block_list *bl,va_list ap)	// Added by RoVeRT
{
	char *event=va_arg(ap,char *);
	int *c=va_arg(ap,int *);

	if(strcmp(event,((struct mob_data *)bl)->npc_event)==0)
		(*c)++;
	return 0;
}

int buildin_mobcount(struct script_state *st)	// Added by RoVeRT
{
	char *mapname,*event;
	int m,c=0;
	mapname=conv_str(st,& (st->stack->stack_data[st->start+2]));
	event=conv_str(st,& (st->stack->stack_data[st->start+3]));

	if( (m=map_mapname2mapid(mapname))<0 ) {
		push_val(st->stack,C_INT,-1);
		return 0;
	}
	map_foreachinmap(buildin_mobcount_sub, m, BL_MOB, event,&c );

	push_val(st->stack,C_INT, (c));

	return 0;
}
int buildin_marriage(struct script_state *st)
{
	char *partner=conv_str(st,& (st->stack->stack_data[st->start+2]));
	struct map_session_data *sd=script_rid2sd(st);
	struct map_session_data *p_sd=map_nick2sd(partner);

	if(sd==NULL || p_sd==NULL || pc_marriage(sd,p_sd) < 0){
		push_val(st->stack,C_INT,0);
		return 0;
	}
	push_val(st->stack,C_INT,1);
	return 0;
}
int buildin_wedding_effect(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	struct block_list *bl;

	if(sd==NULL) {
		bl=map_id2bl(st->oid);
	} else
		bl=&sd->bl;
	clif_wedding_effect(bl);
	return 0;
}
int buildin_divorce(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	if(sd==NULL || pc_divorce(sd) < 0){
		push_val(st->stack,C_INT,0);
		return 0;
	}
	push_val(st->stack,C_INT,1);
	return 0;
}

int buildin_ispartneron(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	struct map_session_data *p_sd=NULL;

	if(sd==NULL || !pc_ismarried(sd) ||
            (p_sd=map_charid2sd(sd->status.partner_id)) == NULL) {
		push_val(st->stack,C_INT,0);
		return 0;
	}

	push_val(st->stack,C_INT,1);
	return 0;
}

int buildin_getpartnerid(struct script_state *st)
{
    struct map_session_data *sd=script_rid2sd(st);
    if (sd == NULL) {
        push_val(st->stack,C_INT,0);
        return 0;
    }

    push_val(st->stack,C_INT,sd->status.partner_id);
    return 0;
}

int buildin_getchildid(struct script_state *st)
{
    struct map_session_data *sd=script_rid2sd(st);
    if (sd == NULL) {
        push_val(st->stack,C_INT,0);
        return 0;
    }

    push_val(st->stack,C_INT,sd->status.child);
    return 0;
}

int buildin_getmotherid(struct script_state *st)
{
    struct map_session_data *sd=script_rid2sd(st);
    if (sd == NULL) {
        push_val(st->stack,C_INT,0);
        return 0;
    }

    push_val(st->stack,C_INT,sd->status.mother);
    return 0;
}

int buildin_getfatherid(struct script_state *st)
{
    struct map_session_data *sd=script_rid2sd(st);
    if (sd == NULL) {
        push_val(st->stack,C_INT,0);
        return 0;
    }

    push_val(st->stack,C_INT,sd->status.father);
    return 0;
}

int buildin_warppartner(struct script_state *st)
{
	int x,y;
	unsigned short mapindex;
	char *str;
	struct map_session_data *sd=script_rid2sd(st);
	struct map_session_data *p_sd=NULL;

	if(sd==NULL || !pc_ismarried(sd) ||
   	(p_sd=map_charid2sd(sd->status.partner_id)) == NULL) {
		push_val(st->stack,C_INT,0);
		return 0;
	}

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y=conv_num(st,& (st->stack->stack_data[st->start+4]));

	mapindex = mapindex_name2id(str);
	if (mapindex) {
		pc_setpos(p_sd,mapindex,x,y,0);
		push_val(st->stack,C_INT,1);
	} else
		push_val(st->stack,C_INT,0);
	return 0;
}

/*================================================
 * Script for Displaying MOB Information [Valaris]
 *------------------------------------------------
 */
int buildin_strmobinfo(struct script_state *st)
{

	int num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	int class_=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if((class_>=0 && class_<=1000) || class_ >2000)
		return 0;

	switch (num) {
	case 1:
		{
			char *buf;
			buf=(char *) aCallocA(NAME_LENGTH, sizeof(char));
//			buf=mob_db(class_)->name;
// for string assignments you would need to go for c++ [Shinomori]
			memcpy(buf, mob_db(class_)->name, NAME_LENGTH-1);
			push_str(st->stack,C_STR,(unsigned char *) buf);
			break;
		}
	case 2:
		{
			char *buf;
			buf=(char *) aCallocA(NAME_LENGTH, sizeof(char));
//			buf=mob_db(class_).jname;
// for string assignments you would need to go for c++ [Shinomori]
			memcpy(buf,mob_db(class_)->jname, NAME_LENGTH-1);
			push_str(st->stack,C_STR,(unsigned char *) buf);
			break;
		}
	case 3:
		push_val(st->stack,C_INT,mob_db(class_)->lv);
		break;
	case 4:
		push_val(st->stack,C_INT,mob_db(class_)->max_hp);
		break;
	case 5:
		push_val(st->stack,C_INT,mob_db(class_)->max_sp);
		break;
	case 6:
		push_val(st->stack,C_INT,mob_db(class_)->base_exp);
		break;
	case 7:
		push_val(st->stack,C_INT,mob_db(class_)->job_exp);
		break;
	}
	return 0;
}

/*==========================================
 * Summon guardians [Valaris]
 *------------------------------------------
 */
int buildin_guardian(struct script_state *st)
{
	int class_=0,amount=1,x=0,y=0,guardian=0;
	char *str,*map,*event="";

	map	=conv_str(st,& (st->stack->stack_data[st->start+2]));
	x	=conv_num(st,& (st->stack->stack_data[st->start+3]));
	y	=conv_num(st,& (st->stack->stack_data[st->start+4]));
	str	=conv_str(st,& (st->stack->stack_data[st->start+5]));
	class_=conv_num(st,& (st->stack->stack_data[st->start+6]));
	amount=conv_num(st,& (st->stack->stack_data[st->start+7]));
	event=conv_str(st,& (st->stack->stack_data[st->start+8]));
	if( st->end>st->start+9 )
		guardian=conv_num(st,& (st->stack->stack_data[st->start+9]));

	mob_spawn_guardian(map_id2sd(st->rid),map,x,y,str,class_,amount,event,guardian);

	return 0;
}

/*================================================
 * Script for Displaying Guardian Info [Valaris]
 *------------------------------------------------
 */
int buildin_guardianinfo(struct script_state *st)
{
	int guardian=conv_num(st,& (st->stack->stack_data[st->start+2]));
	struct map_session_data *sd=script_rid2sd(st);
	struct guild_castle *gc=guild_mapname2gc(map[sd->bl.m].name);

	if (guardian < 0 || guardian >= MAX_GUARDIANS || gc==NULL)
	{
		push_val(st->stack,C_INT,-1);
		return 0;
	}

	if(gc->guardian[guardian].visible)
		push_val(st->stack,C_INT,gc->guardian[guardian].hp);
	else push_val(st->stack,C_INT,-1);

	return 0;
}
/*==========================================
 * IDからItem名
 *------------------------------------------
 */
int buildin_getitemname(struct script_state *st)
{
	int item_id=0;
	struct item_data *i_data;
	char *item_name;
	struct script_data *data;

	data=&(st->stack->stack_data[st->start+2]);
	get_val(st,data);

	if( data->type==C_STR || data->type==C_CONSTSTR ){
		const char *name=conv_str(st,data);
		struct item_data *item_data = itemdb_searchname(name);
		if( item_data )
			item_id=item_data->nameid;
	}else
		item_id=conv_num(st,data);

	i_data = itemdb_exists(item_id);
	if (i_data == NULL)
	{
		push_str(st->stack,C_CONSTSTR,(unsigned char *) "null");
		return 0;
	}
	item_name=(char *)aCallocA(ITEM_NAME_LENGTH,sizeof(char));

	memcpy(item_name, i_data->jname, ITEM_NAME_LENGTH-1);
	push_str(st->stack,C_STR,(unsigned char *) item_name);
	return 0;
}
/*==========================================
 * Returns number of slots an item has. [Skotlex]
 *------------------------------------------
 */
int buildin_getitemslots(struct script_state *st)
{
	int item_id;
	struct item_data *i_data;

	item_id=conv_num(st,& (st->stack->stack_data[st->start+2]));

	i_data = itemdb_exists(item_id);

	if (i_data)
		push_val(st->stack,C_INT,i_data->slot);
	else
		push_val(st->stack,C_INT,-1);
	return 0;
}

/*==========================================
 * Returns some values of an item [Lupus]
 * Price, Weight, etc...
	getiteminfo(itemID,n), where n
		0 value_buy;
		1 value_sell;
		2 type;
		3 maxchance = Max drop chance of this item e.g. 1 = 0.01% , etc..
				if = 0, then monsters don't drop it at all (rare or a quest item)
				if = 10000, then this item is sold in NPC shops only
		4 sex;
		5 equip;
		6 weight;
		7 atk;
		8 def;
		9 range;
		10 slot;
		11 look;
		12 elv;
		13 wlv;
 *------------------------------------------
 */
int buildin_getiteminfo(struct script_state *st)
{
	int item_id,n;
	int *item_arr;
	struct item_data *i_data;

	item_id	= conv_num(st,& (st->stack->stack_data[st->start+2]));
	n	= conv_num(st,& (st->stack->stack_data[st->start+3]));
	i_data = itemdb_exists(item_id);

	if (i_data && n>=0 && n<14) {
		item_arr = (int*)&i_data->value_buy;
		push_val(st->stack,C_INT,item_arr[n]);
	} else
		push_val(st->stack,C_INT,-1);
	return 0;
}

/*==========================================
 * Returns value from equipped item slot n [Lupus]
	getequipcardid(num,slot)
	where
		num = eqip position slot
		slot = 0,1,2,3 (Card Slot N)

	This func returns CARD ID, 255,254,-255 (for card 0, if the item is produced)
		it's useful when you want to check item cards or if it's signed
	Useful for such quests as "Sign this refined item with players name" etc
		Hat[0] +4 -> Player's Hat[0] +4
 *------------------------------------------
 */
int buildin_getequipcardid(struct script_state *st)
{
	int i,num,slot;
	struct map_session_data *sd;

	num=conv_num(st,& (st->stack->stack_data[st->start+2]));
	slot=conv_num(st,& (st->stack->stack_data[st->start+3]));
	sd=script_rid2sd(st);
	i=pc_checkequip(sd,equip[num-1]);
	if(i >= 0 && slot>=0 && slot<4)
		push_val(st->stack,C_INT,sd->status.inventory[i].card[slot]);
	else
		push_val(st->stack,C_INT,0);

	return 0;
}

/*==========================================
 * petskillbonus [Valaris] //Rewritten by [Skotlex]
 *------------------------------------------
 */

int buildin_petskillbonus(struct script_state *st)
{
	struct pet_data *pd;

	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL || sd->pd==NULL)
		return 0;

	pd=sd->pd;
	if (pd->bonus)
	{ //Clear previous bonus
		if (pd->bonus->timer != -1)
			delete_timer(pd->bonus->timer, pet_skill_bonus_timer);
	} else //init
		pd->bonus = (struct pet_bonus *) aCalloc(1, sizeof(struct pet_bonus));

	pd->bonus->type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	pd->bonus->val=conv_num(st,& (st->stack->stack_data[st->start+3]));
	pd->bonus->duration=conv_num(st,& (st->stack->stack_data[st->start+4]));
	pd->bonus->delay=conv_num(st,& (st->stack->stack_data[st->start+5]));

	if (pd->state.skillbonus == -1)
		pd->state.skillbonus=0;	// waiting state

	// wait for timer to start
	if (battle_config.pet_equip_required && pd->equip == 0)
		pd->bonus->timer=-1;
	else
		pd->bonus->timer=add_timer(gettick()+pd->bonus->delay*1000, pet_skill_bonus_timer, sd->bl.id, 0);

	return 0;
}

/*==========================================
 * pet looting [Valaris] //Rewritten by [Skotlex]
 *------------------------------------------
 */
int buildin_petloot(struct script_state *st)
{
	int max;
	struct pet_data *pd;
	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL || sd->pd==NULL)
		return 0;

	max=conv_num(st,& (st->stack->stack_data[st->start+2]));

	if(max < 1)
		max = 1;	//Let'em loot at least 1 item.
	else if (max > MAX_PETLOOT_SIZE)
		max = MAX_PETLOOT_SIZE;

	pd = sd->pd;
	if (pd->loot != NULL)
	{	//Release whatever was there already and reallocate memory
		pet_lootitem_drop(pd, pd->msd);
		aFree(pd->loot->item);
	}
	else
		pd->loot = (struct pet_loot *)aCalloc(1, sizeof(struct pet_loot));

	pd->loot->item = (struct item *)aCalloc(max,sizeof(struct item));
	memset(pd->loot->item,0,max * sizeof(struct item));

	pd->loot->max=max;
	pd->loot->count = 0;
	pd->loot->weight = 0;

	return 0;
}
/*==========================================
 * PCの所持品情報読み取り
 *------------------------------------------
 */
int buildin_getinventorylist(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	unsigned char card_var[NAME_LENGTH];

	int i,j=0,k;
	if(!sd) return 0;
	for(i=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid > 0 && sd->status.inventory[i].amount > 0){
			pc_setreg(sd,add_str((unsigned char *) "@inventorylist_id")+(j<<24),sd->status.inventory[i].nameid);
			pc_setreg(sd,add_str((unsigned char *) "@inventorylist_amount")+(j<<24),sd->status.inventory[i].amount);
			pc_setreg(sd,add_str((unsigned char *) "@inventorylist_equip")+(j<<24),sd->status.inventory[i].equip);
			pc_setreg(sd,add_str((unsigned char *) "@inventorylist_refine")+(j<<24),sd->status.inventory[i].refine);
			pc_setreg(sd,add_str((unsigned char *) "@inventorylist_identify")+(j<<24),sd->status.inventory[i].identify);
			pc_setreg(sd,add_str((unsigned char *) "@inventorylist_attribute")+(j<<24),sd->status.inventory[i].attribute);
			for (k = 0; k < MAX_SLOTS; k++)
			{
				sprintf(card_var, "@inventorylist_card%d",k+1);
				pc_setreg(sd,add_str(card_var)+(j<<24),sd->status.inventory[i].card[k]);
			}
			j++;
		}
	}
	pc_setreg(sd,add_str((unsigned char *) "@inventorylist_count"),j);
	return 0;
}

int buildin_getskilllist(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	int i,j=0;
	if(!sd) return 0;
	for(i=0;i<MAX_SKILL;i++){
		if(sd->status.skill[i].id > 0 && sd->status.skill[i].lv > 0){
			pc_setreg(sd,add_str((unsigned char *) "@skilllist_id")+(j<<24),sd->status.skill[i].id);
			pc_setreg(sd,add_str((unsigned char *)"@skilllist_lv")+(j<<24),sd->status.skill[i].lv);
			pc_setreg(sd,add_str((unsigned char *)"@skilllist_flag")+(j<<24),sd->status.skill[i].flag);
			j++;
		}
	}
	pc_setreg(sd,add_str((unsigned char *) "@skilllist_count"),j);
	return 0;
}

int buildin_clearitem(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	int i;
	if(sd==NULL) return 0;
	for (i=0; i<MAX_INVENTORY; i++) {
		if (sd->status.inventory[i].amount) {

			//Logs items, got from (N)PC scripts [Lupus]
			if(log_config.pick > 0 ) {
				log_pick(sd, "N", 0, sd->status.inventory[i].nameid, -sd->status.inventory[i].amount, &sd->status.inventory[i]);
			}
			//Logs

			pc_delitem(sd, i, sd->status.inventory[i].amount, 0);
		}
	}
	return 0;
}

/*==========================================
	Disguise Player (returns Mob/NPC ID if success, 0 on fail) [Lupus]
 *------------------------------------------
 */
int buildin_disguise(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	int id;

	id	= conv_num(st,& (st->stack->stack_data[st->start+2]));

	if (!mobdb_checkid(id) && !npcdb_checkid(id)) {
		push_val(st->stack,C_INT,0);
		return 0;
	}

	pc_disguise(sd, id);
	push_val(st->stack,C_INT,id);
	return 0;
}

/*==========================================
	Undisguise Player (returns 1 if success, 0 on fail) [Lupus]
 *------------------------------------------
 */
int buildin_undisguise(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);

	if (sd->disguise) {
		pc_disguise(sd, 0);
		push_val(st->stack,C_INT,0);
	} else {
		push_val(st->stack,C_INT,1);
	}
	return 0;
}

/*==========================================
 * NPCクラスチェンジ
 * classは変わりたいclass
 * typeは通常0なのかな？
 *------------------------------------------
 */
int buildin_classchange(struct script_state *st)
{
	int _class,type;
	struct block_list *bl=map_id2bl(st->oid);

	if(bl==NULL) return 0;

	_class=conv_num(st,& (st->stack->stack_data[st->start+2]));
	type=conv_num(st,& (st->stack->stack_data[st->start+3]));
	clif_class_change(bl,_class,type);
	return 0;
}

/*==========================================
 * NPCから発生するエフェクト
 *------------------------------------------
 */
int buildin_misceffect(struct script_state *st)
{
	int type;

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if(st->oid)
		clif_misceffect2(map_id2bl(st->oid),type);
	else{
		struct map_session_data *sd=script_rid2sd(st);
		if(sd)
			clif_misceffect2(&sd->bl,type);
	}
	return 0;
}
/*==========================================
 * サウンドエフェクト
 *------------------------------------------
 */
int buildin_soundeffect(struct script_state *st)
{

	// Redundn
	struct map_session_data *sd=script_rid2sd(st);
	char *name;
	int type=0;


	name=conv_str(st,& (st->stack->stack_data[st->start+2]));
	type=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if(sd){
		if(!st->rid)
			clif_soundeffect(sd,map_id2bl(st->oid),name,type);
		else{
			clif_soundeffect(sd,&sd->bl,name,type);
		}
	}
	return 0;
}

int soundeffect_sub(struct block_list* bl,va_list ap)
{
	char *name;
	int type;

	nullpo_retr(0, bl);
	nullpo_retr(0, ap);

	name = va_arg(ap,char *);
	type = va_arg(ap,int);

	clif_soundeffect((struct map_session_data *)bl, bl, name, type);

    return 0;
}

int buildin_soundeffectall(struct script_state *st)
{
	// [Lance] - Improved.
	char *name, *map = NULL;
	struct block_list *bl;
	int type, coverage, x0, y0, x1, y1;

	name=conv_str(st,& (st->stack->stack_data[st->start+2]));
	type=conv_num(st,& (st->stack->stack_data[st->start+3]));
	coverage=conv_num(st,& (st->stack->stack_data[st->start+4]));

	if(!st->rid)
		bl = map_id2bl(st->oid);
	else
		bl = &(script_rid2sd(st)->bl);

	if(bl){
		if(coverage < 23){
			clif_soundeffectall(bl,name,type,coverage);
		}else {
			if(st->end > st->start+9){
				map=conv_str(st,& (st->stack->stack_data[st->start+5]));
				x0 = conv_num(st,& (st->stack->stack_data[st->start+6]));
				y0 = conv_num(st,& (st->stack->stack_data[st->start+7]));
				x1 = conv_num(st,& (st->stack->stack_data[st->start+8]));
				y1 = conv_num(st,& (st->stack->stack_data[st->start+9]));
				map_foreachinarea(soundeffect_sub,map_mapname2mapid(map),x0,y0,x1,y1,BL_PC,name,type);
			} else {
				ShowError("buildin_soundeffectall: insufficient arguments for specific area broadcast.\n");
			}
		}
	}
	return 0;
}
/*==========================================
 * pet status recovery [Valaris] / Rewritten by [Skotlex]
 *------------------------------------------
 */
int buildin_petrecovery(struct script_state *st)
{
	struct pet_data *pd;
	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL || sd->pd==NULL)
		return 0;

	pd=sd->pd;

	if (pd->recovery)
	{ //Halt previous bonus
		if (pd->recovery->timer != -1)
			delete_timer(pd->recovery->timer, pet_recovery_timer);
	} else //Init
		pd->recovery = (struct pet_recovery *)aCalloc(1, sizeof(struct pet_recovery));

	pd->recovery->type=conv_num(st,& (st->stack->stack_data[st->start+2]));
	pd->recovery->delay=conv_num(st,& (st->stack->stack_data[st->start+3]));

	pd->recovery->timer=-1;

	return 0;
}

/*==========================================
 * pet healing [Valaris] //Rewritten by [Skotlex]
 *------------------------------------------
 */
int buildin_petheal(struct script_state *st)
{
	struct pet_data *pd;
	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL || sd->pd==NULL)
		return 0;

	pd=sd->pd;
	if (pd->s_skill)
	{ //Clear previous skill
		if (pd->s_skill->timer != -1)
		{
			if (pd->s_skill->id)
				delete_timer(pd->s_skill->timer, pet_skill_support_timer);
			else
				delete_timer(pd->s_skill->timer, pet_heal_timer);
		}
	} else //init memory
		pd->s_skill = (struct pet_skill_support *) aCalloc(1, sizeof(struct pet_skill_support));

	pd->s_skill->id=0; //This id identifies that it IS petheal rather than pet_skillsupport
	//Use the lv as the amount to heal
	pd->s_skill->lv=conv_num(st,& (st->stack->stack_data[st->start+2]));
	pd->s_skill->delay=conv_num(st,& (st->stack->stack_data[st->start+3]));
	pd->s_skill->hp=conv_num(st,& (st->stack->stack_data[st->start+4]));
	pd->s_skill->sp=conv_num(st,& (st->stack->stack_data[st->start+5]));

	//Use delay as initial offset to avoid skill/heal exploits
	if (battle_config.pet_equip_required && pd->equip == 0)
		pd->s_skill->timer=-1;
	else
		pd->s_skill->timer=add_timer(gettick()+pd->s_skill->delay*1000,pet_heal_timer,sd->bl.id,0);

	return 0;
}

/*==========================================
 * pet attack skills [Valaris] //Rewritten by [Skotlex]
 *------------------------------------------
 */
int buildin_petskillattack(struct script_state *st)
{
	struct pet_data *pd;
	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL || sd->pd==NULL)
		return 0;

	pd=sd->pd;
	if (pd->a_skill == NULL)
		pd->a_skill = (struct pet_skill_attack *)aCalloc(1, sizeof(struct pet_skill_attack));

	pd->a_skill->id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	pd->a_skill->lv=conv_num(st,& (st->stack->stack_data[st->start+3]));
	pd->a_skill->div_ = 0;
	pd->a_skill->rate=conv_num(st,& (st->stack->stack_data[st->start+4]));
	pd->a_skill->bonusrate=conv_num(st,& (st->stack->stack_data[st->start+5]));

	return 0;
}

/*==========================================
 * pet attack skills [Valaris]
 *------------------------------------------
 */
int buildin_petskillattack2(struct script_state *st)
{
	struct pet_data *pd;
	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL || sd->pd==NULL)
		return 0;

	pd=sd->pd;
	if (pd->a_skill == NULL)
		pd->a_skill = (struct pet_skill_attack *)aCalloc(1, sizeof(struct pet_skill_attack));

	pd->a_skill->id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	pd->a_skill->lv=conv_num(st,& (st->stack->stack_data[st->start+3]));
	pd->a_skill->div_ = conv_num(st,& (st->stack->stack_data[st->start+4]));
	pd->a_skill->rate=conv_num(st,& (st->stack->stack_data[st->start+5]));
	pd->a_skill->bonusrate=conv_num(st,& (st->stack->stack_data[st->start+6]));

	return 0;
}

/*==========================================
 * pet support skills [Skotlex]
 *------------------------------------------
 */
int buildin_petskillsupport(struct script_state *st)
{
	struct pet_data *pd;
	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL || sd->pd==NULL)
		return 0;

	pd=sd->pd;
	if (pd->s_skill)
	{ //Clear previous skill
		if (pd->s_skill->timer != -1)
		{
			if (pd->s_skill->id)
				delete_timer(pd->s_skill->timer, pet_skill_support_timer);
			else
				delete_timer(pd->s_skill->timer, pet_heal_timer);
		}
	} else //init memory
		pd->s_skill = (struct pet_skill_support *) aCalloc(1, sizeof(struct pet_skill_support));

	pd->s_skill->id=conv_num(st,& (st->stack->stack_data[st->start+2]));
	pd->s_skill->lv=conv_num(st,& (st->stack->stack_data[st->start+3]));
	pd->s_skill->delay=conv_num(st,& (st->stack->stack_data[st->start+4]));
	pd->s_skill->hp=conv_num(st,& (st->stack->stack_data[st->start+5]));
	pd->s_skill->sp=conv_num(st,& (st->stack->stack_data[st->start+6]));

	//Use delay as initial offset to avoid skill/heal exploits
	if (battle_config.pet_equip_required && pd->equip == 0)
		pd->s_skill->timer=-1;
	else
		pd->s_skill->timer=add_timer(gettick()+pd->s_skill->delay*1000,pet_skill_support_timer,sd->bl.id,0);

	return 0;
}

/*==========================================
 * Scripted skill effects [Celest]
 *------------------------------------------
 */
int buildin_skilleffect(struct script_state *st)
{
	struct map_session_data *sd;

	int skillid=conv_num(st,& (st->stack->stack_data[st->start+2]));
	int skilllv=conv_num(st,& (st->stack->stack_data[st->start+3]));
	sd=script_rid2sd(st);

	clif_skill_nodamage(&sd->bl,&sd->bl,skillid,skilllv,1);

	return 0;
}

/*==========================================
 * NPC skill effects [Valaris]
 *------------------------------------------
 */
int buildin_npcskilleffect(struct script_state *st)
{
	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);

	int skillid=conv_num(st,& (st->stack->stack_data[st->start+2]));
	int skilllv=conv_num(st,& (st->stack->stack_data[st->start+3]));
	int x=conv_num(st,& (st->stack->stack_data[st->start+4]));
	int y=conv_num(st,& (st->stack->stack_data[st->start+5]));

	clif_skill_poseffect(&nd->bl,skillid,skilllv,x,y,gettick());

	return 0;
}

/*==========================================
 * Special effects [Valaris]
 *------------------------------------------
 */
int buildin_specialeffect(struct script_state *st)
{
	struct block_list *bl=map_id2bl(st->oid);

	if(bl==NULL)
		return 0;

	clif_specialeffect(bl,conv_num(st,& (st->stack->stack_data[st->start+2])), 0);

	return 0;
}

int buildin_specialeffect2(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);

	if(sd==NULL)
		return 0;

	clif_specialeffect(&sd->bl,conv_num(st,& (st->stack->stack_data[st->start+2])), 0);

	return 0;
}

/*==========================================
 * Nude [Valaris]
 *------------------------------------------
 */

int buildin_nude(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	int i,calcflag=0;

	if(sd==NULL)
		return 0;

	for(i=0;i<11;i++)
		if(sd->equip_index[i] >= 0) {
			if(!calcflag)
				calcflag=1;
			pc_unequipitem(sd,sd->equip_index[i],2);
		}

	if(calcflag)
		status_calc_pc(sd,1);

	return 0;
}

/*==========================================
 * gmcommand [MouseJstr]
 *
 * suggested on the forums...
 * splitted into atcommand & charcommand by [Skotlex]
 *------------------------------------------
 */

int buildin_atcommand(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	char *cmd;

	cmd = conv_str(st,& (st->stack->stack_data[st->start+2]));
	if (st->rid)
		sd = script_rid2sd(st);

	if (sd) is_atcommand(sd->fd, sd, cmd, 99);
	else { //Use a dummy character.
		struct map_session_data dummy_sd;
		struct block_list *bl = NULL;
		memset(&dummy_sd, 0, sizeof(struct map_session_data));
		if (st->oid) bl = map_id2bl(st->oid);
		if (bl) {
			memcpy(&dummy_sd.bl, bl, sizeof(struct block_list));
			if (bl->type == BL_NPC)
				strncpy(dummy_sd.status.name, ((TBL_NPC*)bl)->name, NAME_LENGTH);
		}
		is_atcommand(0, &dummy_sd, cmd, 99);
	}

	return 0;
}

int buildin_charcommand(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	char *cmd;

	cmd = conv_str(st,& (st->stack->stack_data[st->start+2]));

	if (st->rid)
		sd = script_rid2sd(st);

	if (sd) is_charcommand(sd->fd, sd, cmd, 99);
	else { //Use a dummy character.
		struct map_session_data dummy_sd;
		struct block_list *bl = NULL;
		memset(&dummy_sd, 0, sizeof(struct map_session_data));
		if (st->oid) bl = map_id2bl(st->oid);
		if (bl) {
			memcpy(&dummy_sd.bl, bl, sizeof(struct block_list));
			if (bl->type == BL_NPC)
				strncpy(dummy_sd.status.name, ((TBL_NPC*)bl)->name, NAME_LENGTH);
		}
		is_charcommand(0, &dummy_sd, cmd, 99);
	}

	return 0;
}


/*==========================================
 * Displays a message for the player only (like system messages like "you got an apple" )
 *------------------------------------------
 */
int buildin_dispbottom(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	char *message;
	message=conv_str(st,& (st->stack->stack_data[st->start+2]));
	if(sd)
		clif_disp_onlyself(sd,message,(int)strlen(message));
	return 0;
}

/*==========================================
 * All The Players Full Recovery
   (HP/SP full restore and resurrect if need)
 *------------------------------------------
 */
int buildin_recovery(struct script_state *st)
{
	struct map_session_data *sd, **all_sd;
	int i = 0, users;

	all_sd = map_getallusers(&users);

	for (i = 0; i < users; i++)
	{
		sd = all_sd[i];
		sd->status.hp = sd->status.max_hp;
		sd->status.sp = sd->status.max_sp;
		clif_updatestatus(sd, SP_HP);
		clif_updatestatus(sd, SP_SP);
		if(pc_isdead(sd)){
			pc_setstand(sd);
			clif_resurrection(&sd->bl, 1);
		}
		clif_displaymessage(sd->fd,"You have been recovered!");
	}
	return 0;
}
/*==========================================
 * Get your pet info: getpetinfo(n)
 * n -> 0:pet_id 1:pet_class 2:pet_name
	3:friendly 4:hungry
 *------------------------------------------
 */
int buildin_getpetinfo(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	int type=conv_num(st,& (st->stack->stack_data[st->start+2]));

	if(sd && sd->status.pet_id){
		switch(type){
			case 0:
				push_val(st->stack,C_INT,sd->status.pet_id);
				break;
			case 1:
				push_val(st->stack,C_INT,sd->pet.class_);
				break;
			case 2:
				if(sd->pet.name)
				{	//Shamelessly copied from strcharinfo() [Skotlex]
					char *buf;
					buf=(char *)aCallocA(NAME_LENGTH,sizeof(char));
					memcpy(buf, sd->pet.name, NAME_LENGTH-1);
					push_str(st->stack,C_STR,(unsigned char *) buf);
				}
				else
					push_str(st->stack,C_CONSTSTR, (unsigned char *) "null");
				break;
			case 3:
				//if(sd->pet.intimate)
				push_val(st->stack,C_INT,sd->pet.intimate);
				break;
			case 4:
				//if(sd->pet.hungry)
				push_val(st->stack,C_INT,sd->pet.hungry);
				break;
			default:
				push_val(st->stack,C_INT,0);
				break;
		}
	}else{
		push_val(st->stack,C_INT,0);
	}
	return 0;
}
/*==========================================
 * Shows wether your inventory(and equips) contain
   selected card or not.
	checkequipedcard(4001);
 *------------------------------------------
 */
int buildin_checkequipedcard(struct script_state *st)
{
	struct map_session_data *sd=script_rid2sd(st);
	int n,i,c=0;
	c=conv_num(st,& (st->stack->stack_data[st->start+2]));

	if(sd){
		for(i=0;i<MAX_INVENTORY;i++){
			if(sd->status.inventory[i].nameid > 0 && sd->status.inventory[i].amount){
				for(n=0;n<MAX_SLOTS;n++){
					if(sd->status.inventory[i].card[n]==c){
						push_val(st->stack,C_INT,1);
						return 0;
					}
				}
			}
		}
	}
	push_val(st->stack,C_INT,0);
	return 0;
}

int buildin_jump_zero(struct script_state *st) {
	int sel;
	sel=conv_num(st,& (st->stack->stack_data[st->start+2]));
	if(!sel) {
		int pos;
		if( st->stack->stack_data[st->start+3].type!=C_POS ){
			ShowError("script: jump_zero: not label !\n");
			st->state=END;
			return 0;
		}

		pos=conv_num(st,& (st->stack->stack_data[st->start+3]));
		st->pos=pos;
		st->state=GOTO;
		// printf("script: jump_zero: jumpto : %d\n",pos);
	} else {
		// printf("script: jump_zero: fail\n");
	}
	return 0;
}

int buildin_select(struct script_state *st)
{
	char *buf;
	int len,i;
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	if(sd->state.menu_or_input==0){
		st->state=RERUNLINE;
		sd->state.menu_or_input=1;
		for(i=st->start+2,len=16;i<st->end;i++){
			conv_str(st,& (st->stack->stack_data[i]));
			len+=(int)strlen(st->stack->stack_data[i].u.str)+1;
		}
		buf=(char *)aCalloc(len+1,sizeof(char));
		buf[0]=0;
		for(i=st->start+2,len=0;i<st->end;i++){
			strcat(buf,st->stack->stack_data[i].u.str);
			strcat(buf,":");
		}
		clif_scriptmenu(script_rid2sd(st),st->oid,buf);
		aFree(buf);
	} else if(sd->npc_menu==0xff){	// cansel
		sd->state.menu_or_input=0;
		st->state=END;
	} else {
//		pc_setreg(sd,add_str((unsigned char *) "l15"),sd->npc_menu);
		pc_setreg(sd,add_str((unsigned char *) "@menu"),sd->npc_menu);
		sd->state.menu_or_input=0;
		push_val(st->stack,C_INT,sd->npc_menu);
	}
	return 0;
}

/*==========================================
 * GetMapMobs
	returns mob counts on a set map:
	e.g. GetMapMobs("prontera.gat")
	use "this" - for player's map
 *------------------------------------------
 */
int buildin_getmapmobs(struct script_state *st)
{
	char *str=NULL;
	int m=-1,bx,by,i;
	int count=0,c;
	struct block_list *bl;

	str=conv_str(st,& (st->stack->stack_data[st->start+2]));

	if(strcmp(str,"this")==0){
		struct map_session_data *sd=script_rid2sd(st);
		if(sd)
			m=sd->bl.m;
		else{
			push_val(st->stack,C_INT,-1);
			return 0;
		}
	}else
		m=map_mapname2mapid(str);

	if(m < 0){
		push_val(st->stack,C_INT,-1);
		return 0;
	}

	for(by=0;by<=(map[m].ys-1)/BLOCK_SIZE;by++){
		for(bx=0;bx<=(map[m].xs-1)/BLOCK_SIZE;bx++){
			bl = map[m].block_mob[bx+by*map[m].bxs];
			c = map[m].block_mob_count[bx+by*map[m].bxs];
			for(i=0;i<c && bl;i++,bl=bl->next){
				if(bl->x>=0 && bl->x<=map[m].xs-1 && bl->y>=0 && bl->y<=map[m].ys-1)
					count++;
			}
		}
	}
	push_val(st->stack,C_INT,count);
	return 0;
}

/*==========================================
 * movenpc [MouseJstr]
 *------------------------------------------
 */

int buildin_movenpc(struct script_state *st)
{
	struct map_session_data *sd;
	char *map,*npc;
	int x,y;

	sd = script_rid2sd(st);

	map = conv_str(st,& (st->stack->stack_data[st->start+2]));
	x = conv_num(st,& (st->stack->stack_data[st->start+3]));
	y = conv_num(st,& (st->stack->stack_data[st->start+4]));
	npc = conv_str(st,& (st->stack->stack_data[st->start+5]));

	return 0;
}

/*==========================================
 * message [MouseJstr]
 *------------------------------------------
 */

int buildin_message(struct script_state *st)
{
	struct map_session_data *sd;
	char *msg,*player;
	struct map_session_data *pl_sd = NULL;

	sd = script_rid2sd(st);

	player = conv_str(st,& (st->stack->stack_data[st->start+2]));
	msg = conv_str(st,& (st->stack->stack_data[st->start+3]));

	if((pl_sd=map_nick2sd((char *) player)) == NULL)
		return 0;
	clif_displaymessage(pl_sd->fd, msg);

	return 0;
}

/*==========================================
 * npctalk (sends message to surrounding
 * area) [Valaris]
 *------------------------------------------
 */

int buildin_npctalk(struct script_state *st)
{
	char *str;
	char message[255];

	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);
	str=conv_str(st,& (st->stack->stack_data[st->start+2]));

	if(nd) {
		memcpy(message, nd->name, NAME_LENGTH);
		strcat(message," : ");
		strncat(message,str, 254); //Prevent overflow possibility. [Skotlex]
		clif_message(&(nd->bl), message);
	}

	return 0;
}

/*==========================================
 * hasitems (checks to see if player has any
 * items on them, if so will return a 1)
 * [Valaris]
 *------------------------------------------
 */

int buildin_hasitems(struct script_state *st)
{
	int i;
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	for(i=0; i<MAX_INVENTORY; i++) {
		if(sd->status.inventory[i].amount && sd->status.inventory[i].nameid!=2364 && sd->status.inventory[i].nameid!=2365) {
			push_val(st->stack,C_INT,1);
			return 0;
		}
	}

	push_val(st->stack,C_INT,0);

	return 0;
}
// change npc walkspeed [Valaris]
int buildin_npcspeed(struct script_state *st)
{
	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);
	int x=0;

	x=conv_num(st,& (st->stack->stack_data[st->start+2]));

	if(nd) {
		nd->speed=x;
	}

	return 0;
}
// make an npc walk to a position [Valaris]
int buildin_npcwalkto(struct script_state *st)
{
	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);
	int x=0,y=0;

	x=conv_num(st,& (st->stack->stack_data[st->start+2]));
	y=conv_num(st,& (st->stack->stack_data[st->start+3]));

	if(nd) {
		unit_walktoxy(&nd->bl,x,y,0);
	}

	return 0;
}
// stop an npc's movement [Valaris]
int buildin_npcstop(struct script_state *st)
{
	struct npc_data *nd=(struct npc_data *)map_id2bl(st->oid);

	if(nd) {
		unit_stop_walking(&nd->bl,1);
	}

	return 0;
}


/*==========================================
  * getlook char info. getlook(arg)
  *------------------------------------------
  */
int buildin_getlook(struct script_state *st){
        int type,val;
        struct map_session_data *sd;
        sd=script_rid2sd(st);

        type=conv_num(st,& (st->stack->stack_data[st->start+2]));
        val=-1;
        switch(type){
        case LOOK_HAIR:	//1
                val=sd->status.hair;
                break;
        case LOOK_WEAPON: //2
                val=sd->status.weapon;
                break;
        case LOOK_HEAD_BOTTOM: //3
                val=sd->status.head_bottom;
                break;
        case LOOK_HEAD_TOP: //4
                val=sd->status.head_top;
                break;
        case LOOK_HEAD_MID: //5
                val=sd->status.head_mid;
                break;
        case LOOK_HAIR_COLOR: //6
                val=sd->status.hair_color;
                break;
        case LOOK_CLOTHES_COLOR: //7
                val=sd->status.clothes_color;
                break;
        case LOOK_SHIELD: //8
                val=sd->status.shield;
                break;
        case LOOK_SHOES: //9
                break;
        }

        push_val(st->stack,C_INT,val);
        return 0;
}

/*==========================================
  *     get char save point. argument: 0- map name, 1- x, 2- y
  *------------------------------------------
*/
int buildin_getsavepoint(struct script_state *st)
{
	int x,y,type;
	char *mapname;
	struct map_session_data *sd;

	sd=script_rid2sd(st);

	type=conv_num(st,& (st->stack->stack_data[st->start+2]));

	x=sd->status.save_point.x;
	y=sd->status.save_point.y;
	switch(type){
		case 0:
			mapname=(char *) aCallocA(MAP_NAME_LENGTH+1, sizeof(char));
			memcpy(mapname, mapindex_id2name(sd->status.save_point.map), MAP_NAME_LENGTH);
			mapname[MAP_NAME_LENGTH]='\0';
			push_str(st->stack,C_STR,(unsigned char *) mapname);
		break;
		case 1:
			push_val(st->stack,C_INT,x);
		break;
		case 2:
			push_val(st->stack,C_INT,y);
		break;
	}
	return 0;
}

/*==========================================
  * Get position for  char/npc/pet/mob objects. Added by Lorky
  *
  *     int getMapXY(MapName$,MaxX,MapY,type,[CharName$]);
  *             where type:
  *                     MapName$ - String variable for output map name
  *                     MapX     - Integer variable for output coord X
  *                     MapY     - Integer variable for output coord Y
  *                     type     - type of object
  *                                0 - Character coord
  *                                1 - NPC coord
  *                                2 - Pet coord
  *                                3 - Mob coord (not released)
  *                     CharName$ - Name object. If miss or "this" the current object
  *
  *             Return:
  *                     0        - success
  *                     -1       - some error, MapName$,MapX,MapY contains unknown value.
  *------------------------------------------
*/
int buildin_getmapxy(struct script_state *st){
	struct map_session_data *sd=NULL;
        struct npc_data *nd;
        struct pet_data *pd;

	int num;
	char *name;
	char prefix;

	int x,y,type;
	char mapname[MAP_NAME_LENGTH+1];
	memset(mapname, 0, sizeof(mapname));

        if( st->stack->stack_data[st->start+2].type!=C_NAME ){
                ShowWarning("script: buildin_getmapxy: not mapname variable\n");
                push_val(st->stack,C_INT,-1);
                return 0;
        }
        if( st->stack->stack_data[st->start+3].type!=C_NAME ){
                ShowWarning("script: buildin_getmapxy: not mapx variable\n");
                push_val(st->stack,C_INT,-1);
                return 0;
        }
        if( st->stack->stack_data[st->start+4].type!=C_NAME ){
                ShowWarning("script: buildin_getmapxy: not mapy variable\n");
                push_val(st->stack,C_INT,-1);
                return 0;
        }

//??????????? >>>  Possible needly check function parameters on C_STR,C_INT,C_INT <<< ???????????//
	type=conv_num(st,& (st->stack->stack_data[st->start+5]));

        switch (type){
            case 0:                                             //Get Character Position
                    if( st->end>st->start+6 )
                        sd=map_nick2sd(conv_str(st,& (st->stack->stack_data[st->start+6])));
                    else
                        sd=script_rid2sd(st);

                    if ( sd==NULL ) {                   //wrong char name or char offline
                        push_val(st->stack,C_INT,-1);
                        return 0;
                    }


                    x=sd->bl.x;
                    y=sd->bl.y;
                    memcpy(mapname,mapindex_id2name(sd->mapindex), MAP_NAME_LENGTH);
                    break;
            case 1:                                             //Get NPC Position
                    if( st->end > st->start+6 )
                        nd=npc_name2id(conv_str(st,& (st->stack->stack_data[st->start+6])));
                    else
                        nd=(struct npc_data *)map_id2bl(st->oid);

                    if ( nd==NULL ) {                   //wrong npc name or char offline
                        push_val(st->stack,C_INT,-1);
                        return 0;
                    }

                    x=nd->bl.x;
                    y=nd->bl.y;
                    memcpy(mapname, map[nd->bl.m].name, MAP_NAME_LENGTH);
                    break;
            case 2:                                             //Get Pet Position
                    if( st->end>st->start+6 )
                        sd=map_nick2sd(conv_str(st,& (st->stack->stack_data[st->start+6])));
                    else
                        sd=script_rid2sd(st);

                    if ( sd==NULL ) {                   //wrong char name or char offline
                        push_val(st->stack,C_INT,-1);
                        return 0;
                    }

                    pd=sd->pd;

                    if(pd==NULL){                       //pet data not found
                        push_val(st->stack,C_INT,-1);
                        return 0;
                    }
                    x=pd->bl.x;
                    y=pd->bl.y;
                    memcpy(mapname, map[pd->bl.m].name, MAP_NAME_LENGTH);
                    break;

            case 3:                                             //Get Mob Position
                        push_val(st->stack,C_INT,-1);
                        return 0;
            default:                                            //Wrong type parameter
                        push_val(st->stack,C_INT,-1);
                        return 0;
        }

     //Set MapName$
        num=st->stack->stack_data[st->start+2].u.num;
        name=(char *)(str_buf+str_data[num&0x00ffffff].str);
        prefix=*name;

        if( prefix!='$' )
            sd=script_rid2sd(st);
        else
            sd=NULL;

        set_reg(sd,num,name,(void*)mapname);

     //Set MapX
        num=st->stack->stack_data[st->start+3].u.num;
        name=(char *)(str_buf+str_data[num&0x00ffffff].str);
        prefix=*name;

        if( prefix!='$' )
            sd=script_rid2sd(st);
        else
            sd=NULL;
        set_reg(sd,num,name,(void*)x);


     //Set MapY
        num=st->stack->stack_data[st->start+4].u.num;
        name=(char *)(str_buf+str_data[num&0x00ffffff].str);
        prefix=*name;

        if( prefix!='$' )
            sd=script_rid2sd(st);
        else
            sd=NULL;

        set_reg(sd,num,name,(void*)y);

     //Return Success value
        push_val(st->stack,C_INT,0);
        return 0;
}

/*=====================================================
 * Allows players to use a skill - by Qamera
 *-----------------------------------------------------
 */
int buildin_skilluseid (struct script_state *st)
{
   int skid,sklv;
   struct map_session_data *sd;

   skid=conv_num(st,& (st->stack->stack_data[st->start+2]));
   sklv=conv_num(st,& (st->stack->stack_data[st->start+3]));
   sd=script_rid2sd(st);
	if (sd)
	   unit_skilluse_id(&sd->bl,sd->bl.id,skid,sklv);

   return 0;
}

/*=====================================================
 * Allows players to use a skill on a position [Celest]
 *-----------------------------------------------------
 */
int buildin_skillusepos(struct script_state *st)
{
   int skid,sklv,x,y;
   struct map_session_data *sd;

   skid=conv_num(st,& (st->stack->stack_data[st->start+2]));
   sklv=conv_num(st,& (st->stack->stack_data[st->start+3]));
   x=conv_num(st,& (st->stack->stack_data[st->start+4]));
   y=conv_num(st,& (st->stack->stack_data[st->start+5]));

   sd=script_rid2sd(st);
	if (sd)
	   unit_skilluse_pos(&sd->bl,x,y,skid,sklv);

   return 0;
}

/*==========================================
 * Allows player to write NPC logs (i.e. Bank NPC, etc) [Lupus]
 *------------------------------------------
 */
int buildin_logmes(struct script_state *st)
{
	if (log_config.npc <= 0 ) return 0;
	conv_str(st,& (st->stack->stack_data[st->start+2]));
	log_npc(script_rid2sd(st),st->stack->stack_data[st->start+2].u.str);
	return 0;
}

int buildin_summon(struct script_state *st)
{
	int _class, id, timeout=0;
	char *str,*event="";
	struct map_session_data *sd;
	struct mob_data *md;

	sd=script_rid2sd(st);
	if (sd) {
		int tick = gettick();
		str	=conv_str(st,& (st->stack->stack_data[st->start+2]));
		_class=conv_num(st,& (st->stack->stack_data[st->start+3]));
		if( st->end>st->start+4 )
			timeout=conv_num(st,& (st->stack->stack_data[st->start+4]));
		if( st->end>st->start+5 )
			event=conv_str(st,& (st->stack->stack_data[st->start+5]));

		id=mob_once_spawn(sd, "this", 0, 0, str,_class,1,event);
		if((md=(struct mob_data *)map_id2bl(id))){
			md->master_id=sd->bl.id;
			md->special_state.ai=1;
			md->mode=mob_db(md->class_)->mode|0x04;
			md->deletetimer=add_timer(tick+(timeout>0?timeout*1000:60000),mob_timer_delete,id,0);
			clif_misceffect2(&md->bl,344);
		}
		clif_skill_poseffect(&sd->bl,AM_CALLHOMUN,1,sd->bl.x,sd->bl.y,tick);
	}

	return 0;
}

/*==========================================
 * Checks whether it is daytime/nighttime
 *------------------------------------------
 */
int buildin_isnight(struct script_state *st)
{
	push_val(st->stack,C_INT, (night_flag == 1));
	return 0;
}

int buildin_isday(struct script_state *st)
{
	push_val(st->stack,C_INT, (night_flag == 0));
	return 0;
}

/*================================================
 * Check whether another item/card has been
 * equipped - used for 2/15's cards patch [celest]
 *------------------------------------------------
 */
// leave this here, just in case
#if 0
int buildin_isequipped(struct script_state *st)
{
	struct map_session_data *sd;
	int i, j, k, id = 1;
	int ret = -1;

	sd = script_rid2sd(st);

	for (i=0; id!=0; i++) {
		int flag = 0;

		FETCH (i+2, id) else id = 0;
		if (id <= 0)
			continue;

		for (j=0; j<10; j++) {
			int index, type;
			index = sd->equip_index[j];
			if(index < 0) continue;
			if(j == 9 && sd->equip_index[8] == index) continue;
			if(j == 5 && sd->equip_index[4] == index) continue;
			if(j == 6 && (sd->equip_index[5] == index || sd->equip_index[4] == index)) continue;
			type = itemdb_type(id);

			if(sd->inventory_data[index]) {
				if (type == 4 || type == 5) {
					if (sd->inventory_data[index]->nameid == id)
						flag = 1;
				} else if (type == 6) {
					for(k=0; k<sd->inventory_data[index]->slot; k++) {
						if (sd->status.inventory[index].card[0]!=0x00ff &&
							sd->status.inventory[index].card[0]!=0x00fe &&
							sd->status.inventory[index].card[0]!=(short)0xff00 &&
							sd->status.inventory[index].card[k] == id) {
							flag = 1;
							break;
						}
					}
				}
				if (flag) break;
			}
		}
		if (ret == -1)
			ret = flag;
		else
			ret &= flag;
		if (!ret) break;
	}

	push_val(st->stack,C_INT,ret);
	return 0;
}
#endif

/*================================================
 * Check how many items/cards in the list are
 * equipped - used for 2/15's cards patch [celest]
 *------------------------------------------------
 */
int buildin_isequippedcnt(struct script_state *st)
{
	struct map_session_data *sd;
	int i, j, k, id = 1;
	int ret = 0;

	sd = script_rid2sd(st);

	for (i=0; id!=0; i++) {
		FETCH (i+2, id) else id = 0;
		if (id <= 0)
			continue;

		for (j=0; j<10; j++) {
			int index, type;
			index = sd->equip_index[j];
			if(index < 0) continue;
			if(j == 9 && sd->equip_index[8] == index) continue;
			if(j == 5 && sd->equip_index[4] == index) continue;
			if(j == 6 && (sd->equip_index[5] == index || sd->equip_index[4] == index)) continue;
			type = itemdb_type(id);

			if(sd->inventory_data[index]) {
				if (type == 4 || type == 5) {
					if (sd->inventory_data[index]->nameid == id)
						ret++; //[Lupus]
				} else if (type == 6) {
					for(k=0; k<sd->inventory_data[index]->slot; k++) {
						if (sd->status.inventory[index].card[0]!=0x00ff &&
							sd->status.inventory[index].card[0]!=0x00fe &&
							sd->status.inventory[index].card[0]!=(short)0xff00 &&
							sd->status.inventory[index].card[k] == id) {
							ret++; //[Lupus]
						}
					}
				}
			}
		}
	}

	push_val(st->stack,C_INT,ret);
	return 0;
}

/*================================================
 * Check whether another card has been
 * equipped - used for 2/15's cards patch [celest]
 * -- Items checked cannot be reused in another
 * card set to prevent exploits
 *------------------------------------------------
 */
int buildin_isequipped(struct script_state *st)
{
	struct map_session_data *sd;
	int i, j, k, id = 1;
	int index, type, flag;
	int ret = -1;

	sd = script_rid2sd(st);

	if (!sd) { //If the player is not attached it is a script error anyway... but better prevent the map server from crashing...
		push_val(st->stack,C_INT,0);
		return 0;
	}

	for (i=0; id!=0; i++)
	{
		FETCH (i+2, id) else id = 0;
		if (id <= 0)
			continue;

		type = itemdb_type(id);
		flag = 0;
		for (j=0; j<10; j++)
		{
			index = sd->equip_index[j];
			if(index < 0) continue;
			if(j == 9 && sd->equip_index[8] == index) continue;
			if(j == 5 && sd->equip_index[4] == index) continue;
			if(j == 6 && (sd->equip_index[5] == index || sd->equip_index[4] == index)) continue;

			if(!sd->inventory_data[index])
				continue;

			switch (type)
			{
				case 4:
				case 5:
					if (sd->inventory_data[index]->nameid == id)
						flag = 1;
					break;
				case 6:
					if (
						sd->inventory_data[index]->slot == 0 ||
						sd->status.inventory[index].card[0] == 0x00ff ||
						sd->status.inventory[index].card[0] == 0x00fe ||
						sd->status.inventory[index].card[0] == (short)0xff00)
						continue;

					for (k = 0; k < sd->inventory_data[index]->slot; k++)
					{	//New hash system which should support up to 4 slots on any equipment. [Skotlex]
						unsigned int hash = 0;
						if (sd->status.inventory[index].card[k] != id)
							continue;

						hash = 1<<((j<5?j:j-5)*4 + k);
						// check if card is already used by another set
						if ((j<5?sd->setitem_hash:sd->setitem_hash2) & hash)
							continue;

						// We have found a match
						flag = 1;
						// Set hash so this card cannot be used by another
						if (j<5)
							sd->setitem_hash |= hash;
						else
							sd->setitem_hash2 |= hash;
						break;
					}
				//Case 6 end
				break;
			}
			if (flag) break;
		}
		if (ret == -1)
			ret = flag;
		else
			ret &= flag;
		if (!ret) break;
	}

	push_val(st->stack,C_INT,ret);
	return 0;
}

/*================================================
 * Check how many given inserted cards in the CURRENT
 * weapon - used for 2/15's cards patch [Lupus]
 *------------------------------------------------
 */
int buildin_cardscnt(struct script_state *st)
{
	struct map_session_data *sd;
	int i, k, id = 1;
	int ret = 0;
	int index, type;

	sd = script_rid2sd(st);

	for (i=0; id!=0; i++) {
		FETCH (i+2, id) else id = 0;
		if (id <= 0)
			continue;

		index = current_equip_item_index; //we get CURRENT WEAPON inventory index from status.c [Lupus]
		if(index < 0) continue;

		type = itemdb_type(id);

		if(sd->inventory_data[index]) {
			if (type == 4 || type == 5) {
				if (sd->inventory_data[index]->nameid == id)
					ret++;
			} else if (type == 6) {
				for(k=0; k<sd->inventory_data[index]->slot; k++) {
					if (sd->status.inventory[index].card[0]!=0x00ff &&
						sd->status.inventory[index].card[0]!=0x00fe &&
						sd->status.inventory[index].card[0]!=(short)0xff00 &&
						sd->status.inventory[index].card[k] == id) {
						ret++;
					}
				}
			}
		}
	}
	push_val(st->stack,C_INT,ret);
//	push_val(st->stack,C_INT,current_equip_item_index);
	return 0;
}

/*=======================================================
 * Returns the refined number of the current item, or an
 * item with inventory index specified
 *-------------------------------------------------------
 */
int buildin_getrefine(struct script_state *st)
{
	struct map_session_data *sd;
	if ((sd = script_rid2sd(st))!= NULL)
		push_val(st->stack, C_INT, sd->status.inventory[current_equip_item_index].refine);
	return 0;
}

/*=======================================================
 * Allows 2 Parents to adopt a character as a Baby
 *-------------------------------------------------------
 */
int buildin_adopt(struct script_state *st)
{
	int ret;

	char *parent1 = conv_str(st,& (st->stack->stack_data[st->start+2]));
	char *parent2 = conv_str(st,& (st->stack->stack_data[st->start+3]));
	char *child = conv_str(st,& (st->stack->stack_data[st->start+4]));

	struct map_session_data *p1_sd = map_nick2sd(parent1);
	struct map_session_data *p2_sd = map_nick2sd(parent2);
	struct map_session_data *c_sd = map_nick2sd(child);

	if (!p1_sd || !p2_sd || !c_sd ||
		p1_sd->status.base_level < 70 ||
		p2_sd->status.base_level < 70)
		return 0;

	ret = pc_adoption(p1_sd, p2_sd, c_sd);
	push_val(st->stack, C_INT, ret);

	return 0;
}

/*=======================================================
 * Day/Night controls
 *-------------------------------------------------------
 */
int buildin_night(struct script_state *st)
{
	if (night_flag != 1) map_night_timer(night_timer_tid, 0, 0, 1);
	return 0;
}
int buildin_day(struct script_state *st)
{
	if (night_flag != 0) map_day_timer(day_timer_tid, 0, 0, 1);
	return 0;
}

//=======================================================
// Unequip [Spectre]
//-------------------------------------------------------
int buildin_unequip(struct script_state *st)
{
	int i;
	size_t num;
	struct map_session_data *sd;

	num = conv_num(st,& (st->stack->stack_data[st->start+2])) - 1;
	sd=script_rid2sd(st);
	if(sd!=NULL && num<10)
	{
		i=pc_checkequip(sd,equip[num]);
		pc_unequipitem(sd,i,2);
		return 0;
	}
	return 0;
}

//=======================================================
// strlen [Valaris]
//-------------------------------------------------------
int buildin_getstrlen(struct script_state *st) {

	char *str = conv_str(st,& (st->stack->stack_data[st->start+2]));
	int len = (str) ? (int)strlen(str) : 0;

	push_val(st->stack,C_INT,len);
	return 0;
}

//=======================================================
// isalpha [Valaris]
//-------------------------------------------------------
int buildin_charisalpha(struct script_state *st) {

	char *str=conv_str(st,& (st->stack->stack_data[st->start+2]));
	int pos=conv_num(st,& (st->stack->stack_data[st->start+3]));

	int val = ( str && pos>0 && (unsigned int)pos<strlen(str) ) ? isalpha( str[pos] ) : 0;

	push_val(st->stack,C_INT,val);
	return 0;
}

// [Lance]
int buildin_fakenpcname(struct script_state *st)
{
	char *name;
	char *newname;
	int look;
	name = conv_str(st,& (st->stack->stack_data[st->start+2]));
	newname = conv_str(st,& (st->stack->stack_data[st->start+3]));
	look = conv_num(st,& (st->stack->stack_data[st->start+4]));
	if(look > 32767 || look < -32768) {
		ShowError("buildin_fakenpcname: Invalid look value %d\n",look);
		return 1; // Safety measure to prevent runtime errors
	}
	npc_changename(name,newname,(short)look);
	return 0;
}

int buildin_atoi(struct script_state *st) {
	char *value;
	value = conv_str(st,& (st->stack->stack_data[st->start+2]));
	push_val(st->stack, C_INT, atoi(value));
	return 0;
}

//-----------------------------------------------------------------------//
//         BRING STRSTR TO SCRIPTING ENGINE         - LORDALFA  START    //
//-----------------------------------------------------------------------//
int buildin_compare(struct script_state *st)                                 {
   char *message;
   char *cmpstring;
   int j;
   message = conv_str(st,& (st->stack->stack_data[st->start+2]));
   cmpstring = conv_str(st,& (st->stack->stack_data[st->start+3]));
   for (j = 0; message[j]; j++)
    message[j] = tolower(message[j]);
   for (j = 0; cmpstring[j]; j++)
    cmpstring[j] = tolower(cmpstring[j]);
   push_val(st->stack,C_INT,(strstr(message,cmpstring) != NULL));
   return 0;
}

//-----------------------------------------------------------------------//
//         BRING STRSTR TO SCRIPTING ENGINE         - LORDALFA  END      //
//-----------------------------------------------------------------------//
// [zBuffer] List of mathematics commands --->
int buildin_sqrt(struct script_state *st){
	double i, a;
	i = conv_num(st, &(st->stack->stack_data[st->start+2]));
	a = sqrt(i);
	push_val(st->stack, C_INT, (int)a);
	return 0;
}

int buildin_pow(struct script_state *st){
	double i, a, b;
	a = conv_num(st, &(st->stack->stack_data[st->start+2]));
	b = conv_num(st, &(st->stack->stack_data[st->start+3]));
	i = pow(a,b);
	push_val(st->stack, C_INT, (int)i);
	return 0;
}
int buildin_distance(struct script_state *st){
	int x0, y0, x1, y1;

	x0 = conv_num(st, &(st->stack->stack_data[st->start+2]));
	y0 = conv_num(st, &(st->stack->stack_data[st->start+3]));
	x1 = conv_num(st, &(st->stack->stack_data[st->start+4]));
	y1 = conv_num(st, &(st->stack->stack_data[st->start+5]));

	push_val(st->stack, C_INT, distance(x0-x1, y0-y1));
	return 0;
}

// <--- [zBuffer] List of mathematics commands
// [zBuffer] List of dynamic var commands --->
void setd_sub(struct map_session_data *sd, char *varname, int elem, void *value)
{
	set_reg(sd, add_str((unsigned char *) varname)+(elem<<24), varname, value);
	return;
}

int buildin_setd(struct script_state *st)
{
	struct map_session_data *sd=NULL;
	char varname[100], *buffer;
	char *value;
	int elem;
	buffer = conv_str(st, & (st->stack->stack_data[st->start+2]));
	value = conv_str(st,  & (st->stack->stack_data[st->start+3]));

	if(sscanf(buffer, "%[^[][%d]", varname, &elem) < 2)
		elem = 0;

	if(st->rid)
		sd = script_rid2sd(st);

	if(varname[strlen(varname)-1] != '$') {
		setd_sub(sd, varname, elem, (void *)atoi(value));
	} else {
		setd_sub(sd, varname, elem, (void *)value);
	}

	return 0;
}

#ifndef TXT_ONLY
int buildin_query_sql(struct script_state *st) {
	char *name, *query;
	int num, i = 0;
	struct map_session_data *sd = (st->rid)? script_rid2sd(st) : NULL;

	query = conv_str(st,& (st->stack->stack_data[st->start+2]));
    strcpy(tmp_sql, query);
	if(mysql_query(&mmysql_handle,tmp_sql)){
		ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		return 1;
	}

	if(st->end > st->start+3) {
		if(st->stack->stack_data[st->start+3].type != C_NAME){
			ShowWarning("buildin_query_sql: 2nd parameter is not a variable!\n");
		} else {
			num=st->stack->stack_data[st->start+3].u.num;
			name=(char *)(str_buf+str_data[num&0x00ffffff].str);
			if((sql_res = mysql_store_result(&mmysql_handle))){
				if(name[strlen(name)-1] != '$') {
					while(i<128 && (sql_row = mysql_fetch_row(sql_res))){
						setd_sub(sd, name, i, (void *)atoi(sql_row[0]));
						i++;
					}
				} else {
					while(i<128 && (sql_row = mysql_fetch_row(sql_res))){
						setd_sub(sd, name, i, (void *)sql_row[0]);
						i++;
					}
				}
				mysql_free_result(sql_res);
			}
		}
	}

	return 0;
}

//Allows escaping of a given string.
int buildin_escape_sql(struct script_state *st) {
	char *t_query, *query;
	query = conv_str(st,& (st->stack->stack_data[st->start+2]));

	t_query = aCallocA(strlen(query)*2+1,sizeof(char));
	jstrescapecpy(t_query,query);
	push_str(st->stack,C_STR,(unsigned char *)t_query);
	return 0;
}
#endif

int buildin_getd (struct script_state *st)
{
	char varname[100], *buffer;
	//struct script_data dat;
	int elem;

	buffer = conv_str(st, & (st->stack->stack_data[st->start+2]));

	if(sscanf(buffer, "%[^[][%d]", varname, &elem) < 2)
		elem = 0;

	/*dat.type=C_NAME;
	dat.u.num=add_str((unsigned char *) varname)+(elem<<24);
	get_val(st,&dat);

	if(dat.type == C_INT)
		push_val(st->stack, C_INT, dat.u.num);
	else if(dat.type == C_CONSTSTR){
		buffer = aStrdup((char *)dat.u.str);
		// dat.u.str holds the actual pointer to the data, must be duplicated.
		// It will be freed later. Tested.
		push_str(st->stack, C_STR, buffer);
	}*/

	// Push the 'pointer' so it's more flexible [Lance]
	push_val(st->stack,C_NAME,
				(elem<<24) | add_str((unsigned char *) varname));

	return 0;
}

// <--- [zBuffer] List of dynamic var commands
// Pet stat [Lance]
int buildin_petstat(struct script_state *st){
	struct map_session_data *sd = NULL;
	char *tmp;
	int flag = conv_num(st, & (st->stack->stack_data[st->start+2]));
	sd = script_rid2sd(st);
	if(!sd || !sd->pet.pet_id){
		if(flag == 2)
			push_str(st->stack, C_CONSTSTR, "");
		else
			push_val(st->stack, C_INT, 0);
	}
	else {
		switch(flag){
			case 1:
				push_val(st->stack, C_INT, (int)sd->pet.class_);
				break;
			case 2:
				tmp = aStrdup(sd->pet.name);
				push_str(st->stack, C_STR, tmp);
				break;
			case 3:
				push_val(st->stack, C_INT, (int)sd->pet.level);
				break;
			case 4:
				push_val(st->stack, C_INT, (int)sd->pet.hungry);
				break;
			case 5:
				push_val(st->stack, C_INT, (int)sd->pet.intimate);
				break;
			default:
				push_val(st->stack, C_INT, 0);
				break;
		}
	}
	return 0;
}

int buildin_callshop(struct script_state *st)
{
	struct map_session_data *sd = NULL;
	struct npc_data *nd;
	char *shopname;
	int flag = 0;
	sd = script_rid2sd(st);
	if (!sd) {
		push_val(st->stack,C_INT,0);
		return 0;
	}
	shopname = conv_str(st, & (st->stack->stack_data[st->start+2]));
	if( st->end>st->start+3 )
		flag = conv_num(st, & (st->stack->stack_data[st->start+3]));
	nd = npc_name2id(shopname);
	if (!nd || nd->bl.type!=BL_NPC || nd->bl.subtype!=SHOP) {
		ShowError("buildin_callshop: Shop [%s] not found (or NPC is not shop type)", shopname);
		push_val(st->stack,C_INT,0);
		return 1;
	}

	switch (flag) {
		case 1: //Buy window
			npc_buysellsel(sd,nd->bl.id,0);
		break;
		case 2: //Sell window
			npc_buysellsel(sd,nd->bl.id,1);
		break;
		default: //Show menu
			clif_npcbuysell(sd,nd->bl.id);
		break;
	}
	sd->npc_shopid = nd->bl.id;
	push_val(st->stack,C_INT,1);
	return 0;
}
/*==========================================
 * Returns some values of an item [Lupus]
 * Price, Weight, etc...
	setiteminfo(itemID,"{new item bonus script}");
 *------------------------------------------
 */
int buildin_setitemscript(struct script_state *st)
{
	int item_id;
	char *script;
	struct item_data *i_data;

	item_id	= conv_num(st,& (st->stack->stack_data[st->start+2]));
	script = conv_str(st,& (st->stack->stack_data[st->start+3]));
	i_data = itemdb_exists(item_id);

	if (i_data && script!=NULL && script[0]=='{') {
		if(i_data->script!=NULL)
			aFree(i_data->script);
		i_data->script = parse_script((unsigned char *) script, 0);
		push_val(st->stack,C_INT,1);
	} else
		push_val(st->stack,C_INT,0);
	return 0;
}

/* Work In Progress [Lupus]
int buildin_addmonsterdrop(struct script_state *st)
{
	int class_,item_id,chance;
	class_=conv_num(st,& (st->stack->stack_data[st->start+2]));
	item_id=conv_num(st,& (st->stack->stack_data[st->start+3]));
	chance=conv_num(st,& (st->stack->stack_data[st->start+4]));
	if(class_>1000 && item_id>500 && chance>0) {
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}
}

int buildin_delmonsterdrop(struct script_state *st)
{
	int class_,item_id;
	class_=conv_num(st,& (st->stack->stack_data[st->start+2]));
	item_id=conv_num(st,& (st->stack->stack_data[st->start+3]));
	if(class_>1000 && item_id>500) {
		push_val(st->stack,C_INT,1);
	} else {
		push_val(st->stack,C_INT,0);
	}
}
*/
/*==========================================
 * Returns some values of a monster [Lupus]
 * Name, Level, race, size, etc...
	getmonsterinfo(monsterID,queryIndex);
 *------------------------------------------
 */
int buildin_getmonsterinfo(struct script_state *st)
{
	struct mob_db *mob;
	int mob_id;

	mob_id	= conv_num(st,& (st->stack->stack_data[st->start+2]));
	if (mob_id <= 1000 || (mob = mob_db(mob_id))==NULL) {
		ShowError("buildin_getmonsterinfo: Wrong Monster ID: %i", mob_id);
		push_val(st->stack, C_INT, -1);
		return -1;
	}

	switch ( conv_num(st,& (st->stack->stack_data[st->start+3])) ) {
		case 0: //Name
			push_str(st->stack,C_CONSTSTR, (unsigned char *) mob->jname);
			break;
		case 1: //Lvl
			push_val(st->stack,C_INT, mob->lv);
			break;
		case 2: //MaxHP
			push_val(st->stack,C_INT, mob->max_hp);
			break;
		case 3: //Base EXP
			push_val(st->stack,C_INT, mob->base_exp);
			break;
		case 4: //Job EXP
			push_val(st->stack,C_INT, mob->job_exp);
			break;
		case 5: //Atk1
			push_val(st->stack,C_INT, mob->atk1);
			break;
		case 6: //Atk2
			push_val(st->stack,C_INT, mob->atk2);
			break;
		case 7: //Def
			push_val(st->stack,C_INT, mob->def);
			break;
		case 8: //Mdef
			push_val(st->stack,C_INT, mob->mdef);
			break;
		case 9: //Str
			push_val(st->stack,C_INT, mob->str);
			break;
		case 10: //Agi
			push_val(st->stack,C_INT, mob->agi);
			break;
		case 11: //Vit
			push_val(st->stack,C_INT, mob->vit);
			break;
		case 12: //Int
			push_val(st->stack,C_INT, mob->int_);
			break;
		case 13: //Dex
			push_val(st->stack,C_INT, mob->dex);
			break;
		case 14: //Luk
			push_val(st->stack,C_INT, mob->luk);
			break;
		case 15: //Range
			push_val(st->stack,C_INT, mob->range);
			break;
		case 16: //Range2
			push_val(st->stack,C_INT, mob->range2);
			break;
		case 17: //Range3
			push_val(st->stack,C_INT, mob->range3);
			break;
		case 18: //Size
			push_val(st->stack,C_INT, mob->size);
			break;
		case 19: //Race
			push_val(st->stack,C_INT, mob->race);
			break;
		case 20: //Element
			push_val(st->stack,C_INT, mob->element);
			break;
		case 21: //Mode
			push_val(st->stack,C_INT, mob->mode);
			break;
		default: //wrong Index
			push_val(st->stack,C_INT,-1);
	}
	return 0;
}

//
// 実行部main
//
/*==========================================
 * コマンドの読み取り
 *------------------------------------------
 */
static int unget_com_data=-1;
int get_com(unsigned char *script,int *pos)
{
	int i,j;
	if(unget_com_data>=0){
		i=unget_com_data;
		unget_com_data=-1;
		return i;
	}
	if(script[*pos]>=0x80){
		return C_INT;
	}
	i=0; j=0;
	while(script[*pos]>=0x40){
		i=script[(*pos)++]<<j;
		j+=6;
	}
	return i+(script[(*pos)++]<<j);
}

/*==========================================
 * コマンドのプッシュバック
 *------------------------------------------
 */
void unget_com(int c)
{
	if(unget_com_data!=-1){
		if(battle_config.error_log)
			ShowError("unget_com can back only 1 data\n");
	}
	unget_com_data=c;
}

/*==========================================
 * 数値の所得
 *------------------------------------------
 */
int get_num(unsigned char *script,int *pos)
{
	int i,j;
	i=0; j=0;
	while(script[*pos]>=0xc0){
		i+=(script[(*pos)++]&0x7f)<<j;
		j+=6;
	}
	return i+((script[(*pos)++]&0x7f)<<j);
}

/*==========================================
 * スタックから値を取り出す
 *------------------------------------------
 */
int pop_val(struct script_state* st)
{
	if(st->stack->sp<=0)
		return 0;
	st->stack->sp--;
	get_val(st,&(st->stack->stack_data[st->stack->sp]));
	if(st->stack->stack_data[st->stack->sp].type==C_INT)
		return st->stack->stack_data[st->stack->sp].u.num;
	return 0;
}

#define isstr(c) ((c).type==C_STR || (c).type==C_CONSTSTR)

/*==========================================
 * 加算演算子
 *------------------------------------------
 */
void op_add(struct script_state* st)
{
	st->stack->sp--;
	get_val(st,&(st->stack->stack_data[st->stack->sp]));
	get_val(st,&(st->stack->stack_data[st->stack->sp-1]));

	if(isstr(st->stack->stack_data[st->stack->sp]) || isstr(st->stack->stack_data[st->stack->sp-1])){
		conv_str(st,&(st->stack->stack_data[st->stack->sp]));
		conv_str(st,&(st->stack->stack_data[st->stack->sp-1]));
	}
	if(st->stack->stack_data[st->stack->sp].type==C_INT){ // ii
		st->stack->stack_data[st->stack->sp-1].u.num += st->stack->stack_data[st->stack->sp].u.num;
	} else { // ssの予定
		char *buf;
		buf=(char *)aCallocA(strlen(st->stack->stack_data[st->stack->sp-1].u.str)+
				strlen(st->stack->stack_data[st->stack->sp].u.str)+1,sizeof(char));
		strcpy(buf,st->stack->stack_data[st->stack->sp-1].u.str);
		strcat(buf,st->stack->stack_data[st->stack->sp].u.str);
		if(st->stack->stack_data[st->stack->sp-1].type==C_STR)
		{
			aFree(st->stack->stack_data[st->stack->sp-1].u.str);
			st->stack->stack_data[st->stack->sp-1].type=C_INT;
		}
		if(st->stack->stack_data[st->stack->sp].type==C_STR)
		{
			aFree(st->stack->stack_data[st->stack->sp].u.str);
			st->stack->stack_data[st->stack->sp].type=C_INT;
		}
		st->stack->stack_data[st->stack->sp-1].type=C_STR;
		st->stack->stack_data[st->stack->sp-1].u.str=buf;
	}
}

/*==========================================
 * 二項演算子(文字列)
 *------------------------------------------
 */
void op_2str(struct script_state *st,int op,int sp1,int sp2)
{
	char *s1=st->stack->stack_data[sp1].u.str,
		 *s2=st->stack->stack_data[sp2].u.str;
	int a=0;

	switch(op){
	case C_EQ:
		a= (strcmp(s1,s2)==0);
		break;
	case C_NE:
		a= (strcmp(s1,s2)!=0);
		break;
	case C_GT:
		a= (strcmp(s1,s2)> 0);
		break;
	case C_GE:
		a= (strcmp(s1,s2)>=0);
		break;
	case C_LT:
		a= (strcmp(s1,s2)< 0);
		break;
	case C_LE:
		a= (strcmp(s1,s2)<=0);
		break;
	default:
		ShowWarning("script: illegal string operator\n");
		break;
	}

	// Because push_val() overwrite stack_data[sp1], C_STR on stack_data[sp1] won't be freed.
	// So, call push_val() after freeing strings. [jA1783]
	// push_val(st->stack,C_INT,a);
	if(st->stack->stack_data[sp1].type==C_STR)
	{
		aFree(s1);
		st->stack->stack_data[sp1].type=C_INT;
	}
	if(st->stack->stack_data[sp2].type==C_STR)
	{
		aFree(s2);
		st->stack->stack_data[sp2].type=C_INT;
	}
	push_val(st->stack,C_INT,a);
}
/*==========================================
 * 二項演算子(数値)
 *------------------------------------------
 */
void op_2num(struct script_state *st,int op,int i1,int i2)
{
	switch(op){
	case C_SUB:
		i1-=i2;
		break;
	case C_MUL:
		{
	#ifndef _MSC_VER
		long long res = i1 * i2;
	#else
		__int64 res = i1 * i2;
	#endif
		if (res >  2147483647 )
			i1 = 2147483647;
		else
			i1*=i2;
		}
		break;
	case C_DIV:
		if (i2 != 0)
			i1/=i2;
		else
			ShowWarning("op_2num: Attempted to divide by 0 in a script (operation C_DIV)!\n");
		break;
	case C_MOD:
		if (i2 != 0)
			i1%=i2;
		else
			ShowWarning("op_2num: Attempted to divide by 0 in a script (operation C_MOD)!\n");
		break;
	case C_AND:
		i1&=i2;
		break;
	case C_OR:
		i1|=i2;
		break;
	case C_XOR:
		i1^=i2;
		break;
	case C_LAND:
		i1=i1&&i2;
		break;
	case C_LOR:
		i1=i1||i2;
		break;
	case C_EQ:
		i1=i1==i2;
		break;
	case C_NE:
		i1=i1!=i2;
		break;
	case C_GT:
		i1=i1>i2;
		break;
	case C_GE:
		i1=i1>=i2;
		break;
	case C_LT:
		i1=i1<i2;
		break;
	case C_LE:
		i1=i1<=i2;
		break;
	case C_R_SHIFT:
		i1=i1>>i2;
		break;
	case C_L_SHIFT:
		i1=i1<<i2;
		break;
	}
	push_val(st->stack,C_INT,i1);
}
/*==========================================
 * 二項演算子
 *------------------------------------------
 */
void op_2(struct script_state *st,int op)
{
	int i1,i2;
	char *s1=NULL,*s2=NULL;

	i2=pop_val(st);
	if( isstr(st->stack->stack_data[st->stack->sp]) )
		s2=st->stack->stack_data[st->stack->sp].u.str;

	i1=pop_val(st);
	if( isstr(st->stack->stack_data[st->stack->sp]) )
		s1=st->stack->stack_data[st->stack->sp].u.str;

	if( s1!=NULL && s2!=NULL ){
		// ss => op_2str
		op_2str(st,op,st->stack->sp,st->stack->sp+1);
	}else if( s1==NULL && s2==NULL ){
		// ii => op_2num
		op_2num(st,op,i1,i2);
	}else{
		// si,is => error
		ShowWarning("script: op_2: int&str, str&int not allow.");
		push_val(st->stack,C_INT,0);
	}
}

/*==========================================
 * 単項演算子
 *------------------------------------------
 */
void op_1num(struct script_state *st,int op)
{
	int i1;
	i1=pop_val(st);
	switch(op){
	case C_NEG:
		i1=-i1;
		break;
	case C_NOT:
		i1=~i1;
		break;
	case C_LNOT:
		i1=!i1;
		break;
	}
	push_val(st->stack,C_INT,i1);
}


/*==========================================
 * 関数の実行
 *------------------------------------------
 */
int run_func(struct script_state *st)
{
	int i,start_sp,end_sp,func;

	end_sp=st->stack->sp;
	for(i=end_sp-1;i>=0 && st->stack->stack_data[i].type!=C_ARG;i--);
	if(i==0){
		if(battle_config.error_log)
			ShowError("function not found\n");
//		st->stack->sp=0;
		st->state=END;
		report_src(st);
		return 1;
	}
	start_sp=i-1;
	st->start=i-1;
	st->end=end_sp;

	func=st->stack->stack_data[st->start].u.num;
	if( st->stack->stack_data[st->start].type!=C_NAME || str_data[func].type!=C_FUNC ){
		ShowMessage ("run_func: '"CL_WHITE"%s"CL_RESET"' (type %d) is not function and command!\n",
				str_buf + str_data[func].str, str_data[func].type);
//		st->stack->sp=0;
		st->state=END;
		report_src(st);
		return 1;
	}
#ifdef DEBUG_RUN
	if(battle_config.etc_log) {
		ShowDebug("run_func : %s? (%d(%d)) sp=%d (%d...%d)\n",str_buf+str_data[func].str, func, str_data[func].type, st->stack->sp, st->start, st->end);
		ShowDebug("stack dump :");
		for(i=0;i<end_sp;i++){
			switch(st->stack->stack_data[i].type){
			case C_INT:
				printf(" int(%d)",st->stack->stack_data[i].u.num);
				break;
			case C_NAME:
				printf(" name(%s)",str_buf+str_data[st->stack->stack_data[i].u.num].str);
				break;
			case C_ARG:
				printf(" arg");
				break;
			case C_POS:
				printf(" pos(%d)",st->stack->stack_data[i].u.num);
				break;
			case C_STR:
				printf(" str(%s)",st->stack->stack_data[i].u.str);
				break;
			case C_CONSTSTR:
				printf(" cstr(%s)",st->stack->stack_data[i].u.str);
				break;
			default:
				printf(" %d,%d",st->stack->stack_data[i].type,st->stack->stack_data[i].u.num);
			}
		}
		printf("\n");
	}
#endif
	if(str_data[func].func){
		if (str_data[func].func(st)) //Report error
			report_src(st);
	} else {
		if(battle_config.error_log)
			ShowError("run_func : %s? (%d(%d))\n",str_buf+str_data[func].str,func,str_data[func].type);
		push_val(st->stack,C_INT,0);
		report_src(st);
	}

	// Stack's datum are used when re-run functions [Eoe]
	if(st->state != RERUNLINE) {
		pop_stack(st->stack,start_sp,end_sp);
	}

	if(st->state==RETFUNC){
		// ユーザー定義関数からの復帰
		int olddefsp=st->stack->defsp;
		int i;

		pop_stack(st->stack,st->stack->defsp,start_sp);	// 復帰に邪魔なスタック削除
		if(st->stack->defsp<4 || st->stack->stack_data[st->stack->defsp-1].type!=C_RETINFO){
			ShowWarning("script:run_func(return) return without callfunc or callsub!\n");
			st->state=END;
			report_src(st);
			return 1;
		}
		i = conv_num(st,& (st->stack->stack_data[st->stack->defsp-4]));					// 引数の数所得
		st->pos=conv_num(st,& (st->stack->stack_data[st->stack->defsp-1]));				// スクリプト位置の復元
		st->script=(char*)conv_num(st,& (st->stack->stack_data[st->stack->defsp-2]));	// スクリプトを復元
		st->stack->defsp=conv_num(st,& (st->stack->stack_data[st->stack->defsp-3]));	// 基準スタックポインタを復元

		pop_stack(st->stack,olddefsp-4-i,olddefsp);		// 要らなくなったスタック(引数と復帰用データ)削除

		st->state=GOTO;
	}

	return 0;
}

/*==========================================
 * スクリプトの実行メイン部分
 *------------------------------------------
 */
int run_script_main(struct script_state *st)
{
	int c/*,rerun_pos*/;
	int cmdcount=script_config.check_cmdcount;
	int gotocount=script_config.check_gotocount;
	struct script_stack *stack=st->stack;

	if(st->state == RERUNLINE) {
		st->state = RUN;
		run_func(st);
		if(st->state == GOTO){
			st->state = RUN;
		}
	} else {
		st->state = RUN;
	}
	while( st->state == RUN) {
		c= get_com((unsigned char *) st->script,&st->pos);
		switch(c){
		case C_EOL:
			if(stack->sp!=stack->defsp){
				if(stack->sp > stack->defsp)
				{	//sp > defsp is valid in cases when you invoke functions and don't use the returned value. [Skotlex]
					//Since sp is supposed to be defsp in these cases, we could assume the extra stack elements are unneeded.
					if (battle_config.etc_log)
						ShowWarning("Clearing unused stack stack.sp(%d) -> default(%d)\n",stack->sp,stack->defsp);
					pop_stack(stack, stack->defsp, stack->sp); //Clear out the unused stack-section.
				} else if(battle_config.error_log)
					ShowError("stack.sp(%d) != default(%d)\n",stack->sp,stack->defsp);
				stack->sp=stack->defsp;
			}
			// rerun_pos=st->pos;
			break;
		case C_INT:
			push_val(stack,C_INT,get_num((unsigned char *) st->script,&st->pos));
			break;
		case C_POS:
		case C_NAME:
			push_val(stack,c,(*(int*)(st->script+st->pos))&0xffffff);
			st->pos+=3;
			break;
		case C_ARG:
			push_val(stack,c,0);
			break;
		case C_STR:
			push_str(stack,C_CONSTSTR,(unsigned char *) (st->script+st->pos));
			while(st->script[st->pos++]);
			break;
		case C_FUNC:
			run_func(st);
			if(st->state==GOTO){
				// rerun_pos=st->pos;
				st->state=0;
				if( gotocount>0 && (--gotocount)<=0 ){
					ShowError("run_script: infinity loop !\n");
					st->state=END;
				}
			}
			break;

		case C_ADD:
			op_add(st);
			break;

		case C_SUB:
		case C_MUL:
		case C_DIV:
		case C_MOD:
		case C_EQ:
		case C_NE:
		case C_GT:
		case C_GE:
		case C_LT:
		case C_LE:
		case C_AND:
		case C_OR:
		case C_XOR:
		case C_LAND:
		case C_LOR:
		case C_R_SHIFT:
		case C_L_SHIFT:
			op_2(st,c);
			break;

		case C_NEG:
		case C_NOT:
		case C_LNOT:
			op_1num(st,c);
			break;

		case C_NOP:
			st->state=END;
			break;

		default:
			if(battle_config.error_log)
				ShowError("unknown command : %d @ %d\n",c,pos);
			st->state=END;
			break;
		}
		if( cmdcount>0 && (--cmdcount)<=0 ){
			ShowError("run_script: infinity loop !\n");
			st->state=END;
		}
	}
	switch(st->state){
	case STOP:
		break;
	case END:
		{
			struct map_session_data *sd=map_id2sd(st->rid);
			st->pos=-1;
			if(sd && sd->npc_id==st->oid)
				npc_event_dequeue(sd);
		}
		break;
	case RERUNLINE:
		// Do not call function of commands two time! [ Eoe / jA 1094 ]
		// For example: select "1", "2", callsub(...);
		// If current script position is changed, callsub will be called two time.
		//
		// {
		// 	st->pos=rerun_pos;
		// }
		break;
	}

	return 0;
}

/*==========================================
 * スクリプトの実行
 *------------------------------------------
 */
int run_script(unsigned char *script,int pos,int rid,int oid)
{
	struct script_state st;
	struct map_session_data *sd;
	unsigned char *rootscript = script;

	//Variables for backing up the previous script and restore it if needed. [Skotlex]
	unsigned char *bck_script = NULL;
	unsigned char *bck_scriptroot = NULL;
	int bck_scriptstate = 0;
	struct script_stack *bck_stack = NULL;

	if (script == NULL || pos < 0)
		return -1;
	memset(&st, 0, sizeof(struct script_state));

	if ((sd = map_id2sd(rid)) && sd->stack && sd->npc_scriptroot == rootscript){
		// we have a stack for the same script, should continue exec.
		st.script = sd->npc_script;
		st.stack = sd->stack;
		st.state  = sd->npc_scriptstate;
		// and clear vars
		sd->stack           = NULL;
		sd->npc_script      = NULL;
		sd->npc_scriptroot  = NULL;
		sd->npc_scriptstate = 0;
	} else {
		// the script is different, make new script_state and stack
		st.stack = aCalloc (1, sizeof(struct script_stack));
		st.stack->sp = 0;
		st.stack->sp_max = 64;
		st.stack->stack_data = (struct script_data *) aCalloc (st.stack->sp_max,sizeof(st.stack->stack_data[0]));
		st.stack->defsp = st.stack->sp;
		st.state  = RUN;
		st.script = rootscript;

		if (sd && sd->stack) {	// if there's a sd and a stack - back it up and restore it if possible.
			bck_script      = sd->npc_script;
			bck_scriptroot  = sd->npc_scriptroot;
			bck_scriptstate = sd->npc_scriptstate;
			bck_stack = sd->stack;
			sd->stack = NULL;
		}
	}
	st.pos = pos;
	st.rid = rid;
	st.oid = oid;
	// let's run that stuff
	run_script_main(&st);

	sd = map_id2sd(st.rid);
	if (st.state != END && sd) {
		// script is not finished, store data in sd.
		sd->npc_script      = st.script;
		sd->npc_scriptroot  = rootscript;
		sd->npc_scriptstate = st.state;
		sd->stack           = st.stack;
		if (bck_stack) //Get rid of the backup as it can't be restored.
			script_free_stack (bck_stack);
	} else {
		// we are done with stuff, free the stack
		script_free_stack (st.stack);
		// and if there was a sd associated - zero vars.
		if (sd) {
			//Clear or restore previous script.
			sd->npc_script      = bck_script;
			sd->npc_scriptroot  = bck_scriptroot;
			sd->npc_scriptstate = bck_scriptstate;
			sd->stack = bck_stack;
			//Since the script is done, save any changed account variables [Skotlex]
			if (sd->state.reg_dirty&2)
				intif_saveregistry(sd,2);
			if (sd->state.reg_dirty&1)
				intif_saveregistry(sd,1);
		}
	}

	return st.pos;
}


/*==========================================
 * マップ変数の変更
 *------------------------------------------
 */
int mapreg_setreg(int num,int val)
{
#if !defined(TXT_ONLY) && defined(MAPREGSQL)
	int i=num>>24;
	char *name=str_buf+str_data[num&0x00ffffff].str;
	char tmp_str[64];
#endif

	if(val!=0) {

#if !defined(TXT_ONLY) && defined(MAPREGSQL)
		if(name[1] != '@' && idb_get(mapreg_db,num) == NULL) {
			sprintf(tmp_sql,"INSERT INTO `%s`(`%s`,`%s`,`%s`) VALUES ('%s','%d','%d')",mapregsql_db,mapregsql_db_varname,mapregsql_db_index,mapregsql_db_value,jstrescapecpy(tmp_str,name),i,val);
			if(mysql_query(&mmysql_handle,tmp_sql)){
				ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
		}
#endif
		idb_put(mapreg_db,num,(void*)val);
	// else
	} else { // [zBuffer]
#if !defined(TXT_ONLY) && defined(MAPREGSQL)
		if(name[1] != '@') { // Remove from database because it is unused.
			sprintf(tmp_sql,"DELETE FROM `%s` WHERE `%s`='%s' AND `%s`='%d'",mapregsql_db,mapregsql_db_varname,name,mapregsql_db_index,i);
			if(mysql_query(&mmysql_handle,tmp_sql)){
				ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
		}
#endif
		idb_remove(mapreg_db,num);
	}

	mapreg_dirty=1;
	return 0;
}
/*==========================================
 * 文字列型マップ変数の変更
 *------------------------------------------
 */
int mapreg_setregstr(int num,const char *str)
{
	char *p;
#if !defined(TXT_ONLY) && defined(MAPREGSQL)
	char tmp_str[64];
	char tmp_str2[512];
	int i=num>>24; // [zBuffer]
	char *name=str_buf+str_data[num&0x00ffffff].str;
#endif

	if( str==NULL || *str==0 ){
#if !defined(TXT_ONLY) && defined(MAPREGSQL)
		if(name[1] != '@') {
			sprintf(tmp_sql,"DELETE FROM `%s` WHERE `%s`='%s' AND `%s`='%d'",mapregsql_db,mapregsql_db_varname,name,mapregsql_db_index,i);
			if(mysql_query(&mmysql_handle,tmp_sql)){
				ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
		}
#endif
		idb_remove(mapregstr_db,num);
		mapreg_dirty=1;
		return 0;
	}
	p=(char *)aCallocA(strlen(str)+1, sizeof(char));
	strcpy(p,str);

	if (idb_put(mapregstr_db,num,p))
		;
#if !defined(TXT_ONLY) && defined(MAPREGSQL)
	else { //put returned null, so we must insert.
		sprintf(tmp_sql,"INSERT INTO `%s`(`%s`,`%s`,`%s`) VALUES ('%s','%d','%s')",mapregsql_db,mapregsql_db_varname,mapregsql_db_index,mapregsql_db_value,jstrescapecpy(tmp_str,name),i,jstrescapecpy(tmp_str2,p));
		if(mysql_query(&mmysql_handle,tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
	}
#endif
	mapreg_dirty=1;
	return 0;
}

/*==========================================
 * 永続的マップ変数の読み込み
 *------------------------------------------
 */
static int script_load_mapreg(void)
{
#if defined(TXT_ONLY) || !defined(MAPREGSQL)
	FILE *fp;
	char line[1024];

	if( (fp=fopen(mapreg_txt,"rt"))==NULL )
		return -1;

	while(fgets(line,sizeof(line),fp)){
		char buf1[256],buf2[1024],*p;
		int n,v,s,i;
		if( sscanf(line,"%255[^,],%d\t%n",buf1,&i,&n)!=2 &&
			(i=0,sscanf(line,"%[^\t]\t%n",buf1,&n)!=1) )
			continue;
		if( buf1[strlen(buf1)-1]=='$' ){
			if( sscanf(line+n,"%[^\n\r]",buf2)!=1 ){
				ShowError("%s: %s broken data !\n",mapreg_txt,buf1);
				continue;
			}
			p=(char *)aCallocA(strlen(buf2) + 1,sizeof(char));
			strcpy(p,buf2);
			s= add_str((unsigned char *) buf1);
			idb_put(mapregstr_db,(i<<24)|s,p);
		}else{
			if( sscanf(line+n,"%d",&v)!=1 ){
				ShowError("%s: %s broken data !\n",mapreg_txt,buf1);
				continue;
			}
			s= add_str((unsigned char *) buf1);
			idb_put(mapreg_db,(i<<24)|s,(void*)v);
		}
	}
	fclose(fp);
	mapreg_dirty=0;
	return 0;
#else
	// SQL mapreg code start [zBuffer]
	/*
	     0       1       2
	+-------------------------+
	| varname | index | value |
	+-------------------------+
	*/
	int perfomance = gettick_nocache();
	sprintf(tmp_sql,"SELECT * FROM `%s`",mapregsql_db);
	ShowInfo("Querying script_load_mapreg ...\n");
	if(mysql_query(&mmysql_handle, tmp_sql) ) {
		ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		return -1;
	}
	ShowInfo("Success! Returning results ...\n");
	sql_res = mysql_store_result(&mmysql_handle);
	if (sql_res) {
        while ((sql_row = mysql_fetch_row(sql_res))) {
			char buf1[33], *p = NULL;
			int i,v,s;
			strcpy(buf1,sql_row[0]);
			if( buf1[strlen(buf1)-1]=='$' ){
				i = atoi(sql_row[1]);
				p=(char *)aCallocA(strlen(sql_row[2]) + 1,sizeof(char));
				strcpy(p,sql_row[2]);
				s= add_str((unsigned char *) buf1);
				idb_put(mapregstr_db,(i<<24)|s,p);
			}else{
				s= add_str((unsigned char *) buf1);
				v= atoi(sql_row[2]);
				i = atoi(sql_row[1]);
				idb_put(mapreg_db,(i<<24)|s,(void *)v);
			}
	    }
	}
	ShowInfo("Freeing results...\n");
	mysql_free_result(sql_res);
	mapreg_dirty=0;
	perfomance = (gettick_nocache() - perfomance) / 1000;
	ShowInfo("SQL Mapreg Loading Completed Under %d Seconds.\n",perfomance);
	return 0;
#endif /* TXT_ONLY */
}
/*==========================================
 * 永続的マップ変数の書き込み
 *------------------------------------------
 */
static int script_save_mapreg_intsub(DBKey key,void *data,va_list ap)
{
#if defined(TXT_ONLY) || !defined(MAPREGSQL)
	FILE *fp=va_arg(ap,FILE*);
	int num=key.i&0x00ffffff, i=key.i>>24;
	char *name=str_buf+str_data[num].str;
	if( name[1]!='@' ){
		if(i==0)
			fprintf(fp,"%s\t%d\n", name, (int)data);
		else
			fprintf(fp,"%s,%d\t%d\n", name, i, (int)data);
	}
	return 0;
#else
	int num=key.i&0x00ffffff, i=key.i>>24; // [zBuffer]
	char *name=str_buf+str_data[num].str;
	if ( name[1] != '@') {
		sprintf(tmp_sql,"UPDATE `%s` SET `%s`='%d' WHERE `%s`='%s' AND `%s`='%d'",mapregsql_db,mapregsql_db_value,(int)data,mapregsql_db_varname,name,mapregsql_db_index,i);
		if(mysql_query(&mmysql_handle, tmp_sql) ) {
			ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
	}
	return 0;
#endif
}
static int script_save_mapreg_strsub(DBKey key,void *data,va_list ap)
{
#if defined(TXT_ONLY) || !defined(MAPREGSQL)
	FILE *fp=va_arg(ap,FILE*);
	int num=key.i&0x00ffffff, i=key.i>>24;
	char *name=str_buf+str_data[num].str;
	if( name[1]!='@' ){
		if(i==0)
			fprintf(fp,"%s\t%s\n", name, (char *)data);
		else
			fprintf(fp,"%s,%d\t%s\n", name, i, (char *)data);
	}
	return 0;
#else
	char tmp_str2[512];
	int num=key.i&0x00ffffff, i=key.i>>24;
	char *name=str_buf+str_data[num].str;
	if ( name[1] != '@') {
		sprintf(tmp_sql,"UPDATE `%s` SET `%s`='%s' WHERE `%s`='%s' AND `%s`='%d'",mapregsql_db,mapregsql_db_value,jstrescapecpy(tmp_str2,(char *)data),mapregsql_db_varname,name,mapregsql_db_index,i);
		if(mysql_query(&mmysql_handle, tmp_sql) ) {
			ShowSQL("DB error - %s\n",mysql_error(&mmysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
	}
	return 0;
#endif
}
static int script_save_mapreg(void)
{
#if defined(TXT_ONLY) || !defined(MAPREGSQL)
	FILE *fp;
	int lock;

	if( (fp=lock_fopen(mapreg_txt,&lock))==NULL ) {
		ShowError("script_save_mapreg: Unable to lock-open file [%s]\n",mapreg_txt);
		return -1;
	}
	mapreg_db->foreach(mapreg_db,script_save_mapreg_intsub,fp);
	mapregstr_db->foreach(mapregstr_db,script_save_mapreg_strsub,fp);
	lock_fclose(fp,mapreg_txt,&lock);
#else
	int perfomance = gettick_nocache();
	mapreg_db->foreach(mapreg_db,script_save_mapreg_intsub);  // [zBuffer]
	mapregstr_db->foreach(mapregstr_db,script_save_mapreg_strsub);
	perfomance = (gettick_nocache() - perfomance) / 1000;
	ShowInfo("Mapreg saved in %d seconds.\n", perfomance);
#endif
	mapreg_dirty=0;
	return 0;
}
static int script_autosave_mapreg(int tid,unsigned int tick,int id,int data)
{
	if(mapreg_dirty)
		if (script_save_mapreg() == -1)
			ShowError("Failed to save the mapreg data!\n");
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
static int set_posword(char *p)
{
	char* np,* str[15];
	int i=0;
	for(i=0;i<11;i++) {
		if((np=strchr(p,','))!=NULL) {
			str[i]=p;
			*np=0;
			p=np+1;
		} else {
			str[i]=p;
			p+=strlen(p);
		}
		if(str[i])
			strcpy(pos[i],str[i]);
	}
	return 0;
}

int script_config_read_sub(char *cfgName)
{
	int i;
	char line[1024],w1[1024],w2[1024];
	FILE *fp;


	fp = fopen(cfgName, "r");
	if (fp == NULL) {
		ShowError("file not found: [%s]\n", cfgName);
		return 1;
	}
	while (fgets(line, sizeof(line) - 1, fp)) {
		if (line[0] == '/' && line[1] == '/')
			continue;
		i = sscanf(line,"%[^:]: %[^\r\n]",w1,w2);
		if (i != 2)
			continue;
		if(strcmpi(w1,"refine_posword")==0) {
			set_posword(w2);
		}
		else if(strcmpi(w1,"verbose_mode")==0) {
			script_config.verbose_mode = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"warn_func_no_comma")==0) {
			script_config.warn_func_no_comma = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"warn_cmd_no_comma")==0) {
			script_config.warn_cmd_no_comma = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"warn_func_mismatch_paramnum")==0) {
			script_config.warn_func_mismatch_paramnum = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"warn_cmd_mismatch_paramnum")==0) {
			script_config.warn_cmd_mismatch_paramnum = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"check_cmdcount")==0) {
			script_config.check_cmdcount = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"check_gotocount")==0) {
			script_config.check_gotocount = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"event_script_type")==0) {
			script_config.event_script_type = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"event_requires_trigger")==0) {
			script_config.event_requires_trigger = battle_config_switch(w2);
		}
		else if(strcmpi(w1,"die_event_name")==0) {
			strncpy(script_config.die_event_name, w2, NAME_LENGTH-1);
			if (strlen(script_config.die_event_name) != strlen(w2))
				ShowWarning("script_config_read: Event label truncated (max length is 23 chars): %d\n", script_config.die_event_name);
		}
		else if(strcmpi(w1,"kill_event_name")==0) {
			strncpy(script_config.kill_event_name, w2, NAME_LENGTH-1);
			if (strlen(script_config.kill_event_name) != strlen(w2))
				ShowWarning("script_config_read: Event label truncated (max length is 23 chars): %d\n", script_config.kill_event_name);
		}
		else if(strcmpi(w1,"login_event_name")==0) {
			strncpy(script_config.login_event_name, w2, NAME_LENGTH-1);
			if (strlen(script_config.login_event_name) != strlen(w2))
				ShowWarning("script_config_read: Event label truncated (max length is 23 chars): %d\n", script_config.login_event_name);
		}
		else if(strcmpi(w1,"logout_event_name")==0) {
			strncpy(script_config.logout_event_name, w2, NAME_LENGTH-1);
			if (strlen(script_config.logout_event_name) != strlen(w2))
				ShowWarning("script_config_read: Event label truncated (max length is 23 chars): %d\n", script_config.logout_event_name);
		}
		else if(strcmpi(w1,"loadmap_event_name")==0) {
			strncpy(script_config.loadmap_event_name, w2, NAME_LENGTH-1);
			if (strlen(script_config.loadmap_event_name) != strlen(w2))
				ShowWarning("script_config_read: Event label truncated (max length is 23 chars): %d\n", script_config.loadmap_event_name);
		}
		else if(strcmpi(w1,"baselvup_event_name")==0) {
			strncpy(script_config.baselvup_event_name, w2, NAME_LENGTH-1);
			if (strlen(script_config.baselvup_event_name) != strlen(w2))
				ShowWarning("script_config_read: Event label truncated (max length is 23 chars): %d\n", script_config.baselvup_event_name);
		}
		else if(strcmpi(w1,"joblvup_event_name")==0) {
			strncpy(script_config.joblvup_event_name, w2, NAME_LENGTH-1);
			if (strlen(script_config.joblvup_event_name) != strlen(w2))
				ShowWarning("script_config_read: Event label truncated (max length is 23 chars): %d\n", script_config.joblvup_event_name);
		}
		else if(strcmpi(w1,"import")==0){
			script_config_read_sub(w2);
		}
	}
	fclose(fp);

	return 0;
}

int script_config_read(char *cfgName)
{	//Script related variables should be initialized once! [Skotlex]

	memset (&script_config, 0, sizeof(script_config));
	script_config.verbose_mode = 0;
	script_config.warn_func_no_comma = 1;
	script_config.warn_cmd_no_comma = 1;
	script_config.warn_func_mismatch_paramnum = 1;
	script_config.warn_cmd_mismatch_paramnum = 1;
	script_config.check_cmdcount = 65535;
	script_config.check_gotocount = 2048;

	script_config.event_script_type = 0;
	script_config.event_requires_trigger = 1;

	return script_config_read_sub(cfgName);
}

/*==========================================
 * 終了
 *------------------------------------------
 */
int do_final_script()
{
	if(mapreg_dirty>=0)
		script_save_mapreg();

	mapreg_db->destroy(mapreg_db,NULL);
	mapregstr_db->destroy(mapregstr_db,NULL);
	scriptlabel_db->destroy(scriptlabel_db,NULL);
	userfunc_db->destroy(userfunc_db,NULL);

	if (str_data)
		aFree(str_data);
	if (str_buf)
		aFree(str_buf);

	return 0;
}
/*==========================================
 * 初期化
 *------------------------------------------
 */
int do_init_script()
{
	mapreg_db= db_alloc(__FILE__,__LINE__,DB_INT,DB_OPT_BASE,sizeof(int));
	mapregstr_db=db_alloc(__FILE__,__LINE__,DB_INT,DB_OPT_RELEASE_DATA,sizeof(int));
	userfunc_db=db_alloc(__FILE__,__LINE__,DB_STRING,DB_OPT_RELEASE_BOTH,50);
	scriptlabel_db=db_alloc(__FILE__,__LINE__,DB_STRING,DB_OPT_ALLOW_NULL_DATA,50);

	script_load_mapreg();

	add_timer_func_list(script_autosave_mapreg,"script_autosave_mapreg");
	add_timer_interval(gettick()+MAPREG_AUTOSAVE_INTERVAL,
		script_autosave_mapreg,0,0,MAPREG_AUTOSAVE_INTERVAL);

	return 0;
}

int script_reload()
{
	if(mapreg_dirty>=0)
		script_save_mapreg();

	mapreg_db->clear(mapreg_db, NULL);
	mapregstr_db->clear(mapreg_db, NULL);
	userfunc_db->clear(mapreg_db, NULL);
	scriptlabel_db->clear(mapreg_db, NULL);

	script_load_mapreg();
	return 0;
}
