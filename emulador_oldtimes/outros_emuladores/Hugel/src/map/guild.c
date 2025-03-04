// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "../common/cbasetypes.h"
#include "../common/timer.h"
#include "../common/nullpo.h"
#include "../common/malloc.h"
#include "../common/mapindex.h"
#include "../common/showmsg.h"
#include "../common/ers.h"
#include "../common/strlib.h"

#include "map.h"
#include "guild.h"
#include "storage.h"
#include "battle.h"
#include "npc.h"
#include "pc.h"
#include "status.h"
#include "mob.h"
#include "intif.h"
#include "clif.h"
#include "skill.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static DB guild_db;
static DB castle_db;
static DB guild_expcache_db;
static DB guild_infoevent_db;
static DB guild_castleinfoevent_db;

struct eventlist {
	char name[50];
	struct eventlist *next;
};

// ギルドのEXPキャッシュのフラッシュに関連する定数
#define GUILD_SEND_XY_INVERVAL	5000	// 座標やＨＰ送信の間隔
#define GUILD_PAYEXP_INVERVAL 10000	// 間隔(キャッシュの最大生存時間、ミリ秒)
#define GUILD_PAYEXP_LIST 8192	// キャッシュの最大数

// ギルドのEXPキャッシュ
struct guild_expcache {
	int guild_id, account_id, char_id;
	unsigned int exp;
};
static struct eri *expcache_ers; //For handling of guild exp payment.

struct{
	int id;
	int max;
	struct{
		short id;
		short lv;
	}need[6];
} guild_skill_tree[MAX_GUILDSKILL];

// timer for auto saving guild data during WoE
#define GUILD_SAVE_INTERVAL 300000
int guild_save_timer = -1;

int guild_payexp_timer(int tid,unsigned int tick,int id,int data);
int guild_gvg_eliminate_timer(int tid,unsigned int tick,int id,int data);
int guild_save_sub(int tid,unsigned int tick,int id,int data);
static int guild_send_xy_timer(int tid,unsigned int tick,int id,int data);

 // Modified [Komurka]
int guild_skill_get_max (int id)
{
	if (id  < GD_SKILLBASE || id > GD_SKILLBASE+MAX_GUILDSKILL)
		return 0;
	return guild_skill_tree[id-GD_SKILLBASE].max;
}

// ギルドスキルがあるか確認
int guild_checkskill(struct guild *g,int id)
{
	int idx = id-GD_SKILLBASE;
	if (idx < 0 || idx >= MAX_GUILDSKILL)
		return 0;
	return g->skill[idx].lv;
}

/*==========================================
 * guild_skill_tree.txt reading - from jA [Komurka]
 *------------------------------------------*/
int guild_read_guildskill_tree_db(void)
{
	int i,k,id=0,ln=0;
	FILE *fp;
	char line[1024],*p;

	memset(guild_skill_tree,0,sizeof(guild_skill_tree));
	sprintf(line, "%s/guild_skill_tree.txt", db_path);
	if( (fp=fopen(line,"r"))==NULL){
		ShowError("can't read %s\n", line);
		return -1;
	}
	while(fgets(line, sizeof(line), fp))
	{
		char *split[50];
		if(line[0]=='/' && line[1]=='/')
			continue;
		for(i=0,p=line;i<12 && p;i++){
			split[i]=p;
			p=strchr(p,',');
			if(p) *p++=0;
		}
		if(i<12)
			continue;
		id = atoi(split[0]) - GD_SKILLBASE;
		if(id<0 || id>=MAX_GUILDSKILL)
			continue;
		guild_skill_tree[id].id=atoi(split[0]);
		guild_skill_tree[id].max=atoi(split[1]);
		if (guild_skill_tree[id].id==GD_GLORYGUILD && battle_config.require_glory_guild && guild_skill_tree[id].max==0) guild_skill_tree[id].max=1;
		for(k=0;k<5;k++){
			guild_skill_tree[id].need[k].id=atoi(split[k*2+2]);
			guild_skill_tree[id].need[k].lv=atoi(split[k*2+3]);
		}
	ln++;
	}
	fclose(fp);
	ShowStatus("Done reading '"CL_WHITE"%d"CL_RESET"' entries in '"CL_WHITE"%s"CL_RESET"'.\n",ln,"guild_skill_tree.txt");

	return 0;
}

/*==========================================
 * Guild skill check - from jA [Komurka]
 *------------------------------------------*/
int guild_check_skill_require(struct guild *g,int id)
{
	int i;
	int idx = id-GD_SKILLBASE;

	if(g == NULL)
		return 0;

	if (idx < 0 || idx >= MAX_GUILDSKILL)
		return 0;

	for(i=0;i<5;i++)
	{
		if(guild_skill_tree[idx].need[i].id == 0) break;
		if(guild_skill_tree[idx].need[i].lv > guild_checkskill(g,guild_skill_tree[idx].need[i].id))
			return 0;
	}
	return 1;
}

static int guild_read_castledb(void)
{
	FILE *fp;
	char line[1024];
	int j,ln=0;
	char *str[32],*p;
	struct guild_castle *gc;

	sprintf(line, "%s/castle_db.txt", db_path);
	if( (fp=fopen(line,"r"))==NULL){
		ShowError("can't read %s\n", line);
		return -1;
	}

	while(fgets(line, sizeof(line), fp))
	{
		if(line[0]=='/' && line[1]=='/')
			continue;
		memset(str,0,sizeof(str));
		for(j=0,p=line;j<6 && p;j++){
			str[j]=p;
			p=strchr(p,',');
			if(p) *p++=0;
		}
		if (j < 4) //Insufficient data for castle. [Skotlex]
		{
			ShowError("castle_db.txt: invalid line '%s'\n", line);
			continue;
		}

		gc=(struct guild_castle *)aCalloc(1,sizeof(struct guild_castle));
		gc->castle_id=atoi(str[0]);
		mapindex_getmapname(str[1],gc->map_name);
		safestrncpy(gc->castle_name,str[2],NAME_LENGTH);
		safestrncpy(gc->castle_event,str[3],NAME_LENGTH);

		idb_put(castle_db,gc->castle_id,gc);

		//intif_guild_castle_info(gc->castle_id);

		ln++;
	}
	fclose(fp);
	ShowStatus("Done reading '"CL_WHITE"%d"CL_RESET"' entries in '"CL_WHITE"%s"CL_RESET"'.\n",ln,"castle_db.txt");
	return 0;
}

// 初期化
void do_init_guild(void)
{
	guild_db=db_alloc(__FILE__,__LINE__,DB_INT,DB_OPT_RELEASE_DATA,sizeof(int));
	castle_db=db_alloc(__FILE__,__LINE__,DB_INT,DB_OPT_RELEASE_DATA,sizeof(int));
	guild_expcache_db=db_alloc(__FILE__,__LINE__,DB_INT,DB_OPT_BASE,sizeof(int));
	guild_infoevent_db=db_alloc(__FILE__,__LINE__,DB_INT,DB_OPT_BASE,sizeof(int));
	expcache_ers = ers_new(sizeof(struct guild_expcache)); 
	guild_castleinfoevent_db=db_alloc(__FILE__,__LINE__,DB_INT,DB_OPT_BASE,sizeof(int));

	guild_read_castledb();

	guild_read_guildskill_tree_db(); //guild skill tree [Komurka]

	add_timer_func_list(guild_gvg_eliminate_timer,"guild_gvg_eliminate_timer");
	add_timer_func_list(guild_payexp_timer,"guild_payexp_timer");
	add_timer_func_list(guild_save_sub, "guild_save_sub");
	add_timer_func_list(guild_send_xy_timer, "guild_send_xy_timer");
	add_timer_interval(gettick()+GUILD_PAYEXP_INVERVAL,guild_payexp_timer,0,0,GUILD_PAYEXP_INVERVAL);
	add_timer_interval(gettick()+GUILD_SEND_XY_INVERVAL,guild_send_xy_timer,0,0,GUILD_SEND_XY_INVERVAL);
}


// 検索
struct guild *guild_search(int guild_id)
{
	return idb_get(guild_db,guild_id);
}
int guild_searchname_sub(DBKey key,void *data,va_list ap)
{
	struct guild *g=(struct guild *)data,**dst;
	char *str;
	str=va_arg(ap,char *);
	dst=va_arg(ap,struct guild **);
	if(strcmpi(g->name,str)==0)
		*dst=g;
	return 0;
}
// ギルド名検索
struct guild* guild_searchname(char *str)
{
	struct guild *g=NULL;
	guild_db->foreach(guild_db,guild_searchname_sub,str,&g);
	return g;
}
struct guild_castle *guild_castle_search(int gcid)
{
	return idb_get(castle_db,gcid);
}

// mapnameに対応したアジトのgcを返す
struct guild_castle* guild_mapname2gc(const char* mapname)
{
	int i;
	for(i = 0; i < MAX_GUILDCASTLE; i++)
	{
		struct guild_castle* gc;
		gc = guild_castle_search(i);
		if(!gc) continue;
		if(strcmp(gc->map_name,mapname)==0) return gc;
	}
	return NULL;
}

struct guild_castle *guild_mapindex2gc(short mapindex)
{
	int i;
	struct guild_castle *gc=NULL;

	for(i=0;i<MAX_GUILDCASTLE;i++){
		gc=guild_castle_search(i);
		if(!gc) continue;
		if(strcmp(gc->map_name,mapindex_id2name(mapindex))==0) return gc;
	}
	return NULL;
}



// ログイン中のギルドメンバーの１人のsdを返す
struct map_session_data *guild_getavailablesd(struct guild *g)
{
	int i;

	nullpo_retr(NULL, g);

	for(i=0;i<g->max_member;i++)
		if(g->member[i].sd!=NULL)
			return g->member[i].sd;
	return NULL;
}

// ギルドメンバーのインデックスを返す
int guild_getindex(struct guild *g,int account_id,int char_id)
{
	int i;
	if(g==NULL)
		return -1;
	for(i=0;i<g->max_member;i++)
		if( g->member[i].account_id==account_id &&
			g->member[i].char_id==char_id )
			return i;
	return -1;
}
// ギルドメンバーの役職を返す
int guild_getposition(struct map_session_data *sd,struct guild *g)
{
	int i;

	nullpo_retr(-1, sd);

	if(g==NULL && (g=guild_search(sd->status.guild_id))==NULL)
		return -1;
	for(i=0;i<g->max_member;i++)
		if( g->member[i].account_id==sd->status.account_id &&
			g->member[i].char_id==sd->status.char_id )
			return g->member[i].position;
	return -1;
}

// メンバー情報の作成
void guild_makemember(struct guild_member *m,struct map_session_data *sd)
{
	nullpo_retv(sd);

	memset(m,0,sizeof(struct guild_member));
	m->account_id	=sd->status.account_id;
	m->char_id		=sd->status.char_id;
	m->hair			=sd->status.hair;
	m->hair_color	=sd->status.hair_color;
	m->gender		=sd->status.sex;
	m->class_		=sd->status.class_;
	m->lv			=sd->status.base_level;
//	m->exp			=0;
//	m->exp_payper	=0;
	m->online		=1;
	m->position		=MAX_GUILDPOSITION-1;
	memcpy(m->name,sd->status.name,NAME_LENGTH);
	return;
}
// ギルド競合確認
int guild_check_conflict(struct map_session_data *sd)
{
	nullpo_retr(0, sd);

	intif_guild_checkconflict(sd->status.guild_id,
		sd->status.account_id,sd->status.char_id);
	return 0;
}

// ギルドのEXPキャッシュをinter鯖にフラッシュする
int guild_payexp_timer_sub(DBKey dataid, void *data, va_list ap)
{
	int i;
	struct guild_expcache *c;
	struct guild *g;

	c = (struct guild_expcache *)data;
	
	if (
		(g = guild_search(c->guild_id)) == NULL ||
		(i = guild_getindex(g, c->account_id, c->char_id)) < 0
	) {
		ers_free(expcache_ers, data);
		return 0;
	}

	if (g->member[i].exp > UINT_MAX - c->exp)
		g->member[i].exp = UINT_MAX;
	else
		g->member[i].exp+= c->exp;

	intif_guild_change_memberinfo(g->guild_id,c->account_id,c->char_id,
		GMI_EXP,&g->member[i].exp,sizeof(g->member[i].exp));
	c->exp=0;

	ers_free(expcache_ers, data);
	return 0;
}

int guild_payexp_timer(int tid, unsigned int tick, int id, int data)
{
	guild_expcache_db->clear(guild_expcache_db,guild_payexp_timer_sub);
	return 0;
}

//Taken from party_send_xy_timer_sub. [Skotlex]
int guild_send_xy_timer_sub(DBKey key,void *data,va_list ap)
{
	struct guild *g=(struct guild *)data;
	int i;

	nullpo_retr(0, g);

	for(i=0;i<g->max_member;i++){
		struct map_session_data *sd;
		if((sd=g->member[i].sd)!=NULL){
			if(sd->guild_x!=sd->bl.x || sd->guild_y!=sd->bl.y){
				clif_guild_xy(sd);
				sd->guild_x=sd->bl.x;
				sd->guild_y=sd->bl.y;
			}
		}
	}
	return 0;
}

//Code from party_send_xy_timer [Skotlex]
static int guild_send_xy_timer(int tid,unsigned int tick,int id,int data)
{
	guild_db->foreach(guild_db,guild_send_xy_timer_sub,tick);
	return 0;
}

int guild_send_dot_remove(struct map_session_data *sd)
{
	if (sd->status.guild_id)
		clif_guild_xy_remove(sd);
	return 0;
}
//------------------------------------------------------------------------

// 作成要求
int guild_create(struct map_session_data *sd,char *name)
{
	nullpo_retr(0, sd);

	if(sd->status.guild_id)
	{
		clif_guild_created(sd,1);	// すでに所属している
		return 0;
	}
	if(!battle_config.guild_emperium_check || pc_search_inventory(sd,714) >= 0) {
		struct guild_member m;
		guild_makemember(&m,sd);
		m.position=0;
		intif_guild_create(name,&m);
		return 1;
	}
	clif_guild_created(sd,3);	// エンペリウムがいない
	return 0;
}

// 作成可否
int guild_created(int account_id,int guild_id)
{
	struct map_session_data *sd=map_id2sd(account_id);

	if(sd==NULL)
		return 0;
	if(!guild_id) {
		clif_guild_created(sd,2);	// 作成失敗（同名ギルド存在）
		return 0;
	}
	//struct guild *g;
	sd->status.guild_id=guild_id;
	sd->state.guild_sent=0;
	clif_guild_created(sd,0);
	if(battle_config.guild_emperium_check)
		pc_delitem(sd,pc_search_inventory(sd,714),1,0);	// エンペリウム消耗
	return 0;
}

// 情報要求
int guild_request_info(int guild_id)
{
//	if(battle_config.etc_log)
//		printf("guild_request_info\n");
	return intif_guild_request_info(guild_id);
}
// イベント付き情報要求
int guild_npc_request_info(int guild_id,const char *event)
{
	struct eventlist *ev;

	if( guild_search(guild_id) ){
		if(event && *event)
			npc_event_do(event);
		return 0;
	}

	if(event==NULL || *event==0)
		return guild_request_info(guild_id);

	ev=(struct eventlist *)aCalloc(sizeof(struct eventlist),1);
	memcpy(ev->name,event,strlen(event));
	//The one in the db becomes the next event from this.
	ev->next=idb_put(guild_infoevent_db,guild_id,ev);
	return guild_request_info(guild_id);
}

// 所属キャラの確認
int guild_check_member(struct guild *g)
{
	int i, j, users;
	struct map_session_data *sd, **all_sd;

	nullpo_retr(0, g);

	all_sd = map_getallusers(&users);
	
	for(i=0;i<users;i++){
		sd=all_sd[i];
		if(sd->status.guild_id==g->guild_id){
			j=guild_getindex(g,sd->status.account_id,sd->status.char_id);
			if (j < 0) {
				sd->status.guild_id=0;
				sd->state.guild_sent=0;
				sd->guild_emblem_id=0;
				if(battle_config.error_log)
					ShowWarning("guild: check_member %d[%s] is not member\n",sd->status.account_id,sd->status.name);
			}
		}
	}
	return 0;
}
// 情報所得失敗（そのIDのキャラを全部未所属にする）
int guild_recv_noinfo(int guild_id)
{
	int i, users;
	struct map_session_data *sd, **all_sd;

	all_sd = map_getallusers(&users);
	
	for(i=0;i<users;i++){
		if((sd=all_sd[i])){
			if(sd->status.guild_id==guild_id)
				sd->status.guild_id=0;
		}
	}
	return 0;
}
// 情報所得
int guild_recv_info(struct guild *sg)
{
	struct guild *g,before;
	int i,bm,m;
	struct eventlist *ev,*ev2;

	nullpo_retr(0, sg);

	if((g=idb_get(guild_db,sg->guild_id))==NULL){
		struct map_session_data *sd;
		g=(struct guild *)aCalloc(1,sizeof(struct guild));
		idb_put(guild_db,sg->guild_id,g);
		before=*sg;

		// 最初のロードなのでユーザーのチェックを行う
		guild_check_member(sg);
		//If the guild master is online the first time the guild_info is received, that means he was the first to join,
		//and as such, his guild skills should be blocked to avoid login/logout abuse [Skotlex]
		if ((sd = map_nick2sd(sg->master)) != NULL)
		{
			guild_block_skill(sd, 300000);
			//Also set the guild master flag.
			sd->state.gmaster_flag = g;
			clif_charnameupdate(sd); // [LuzZza]			
		}
	}else
		before=*g;
	memcpy(g,sg,sizeof(struct guild));

	if(g->max_member > MAX_GUILD)
	{
		if (battle_config.error_log)
			ShowError("guild_recv_info: Received guild with %d members, but MAX_GUILD is only %d. Extra guild-members have been lost!\n", g->max_member, MAX_GUILD);
		g->max_member = MAX_GUILD;
	}
	
	for(i=bm=m=0;i<g->max_member;i++){	// sdの設定と人数の確認
		if(g->member[i].account_id>0){
			struct map_session_data *sd = map_id2sd(g->member[i].account_id);
			if (sd && sd->status.char_id == g->member[i].char_id &&
				sd->status.guild_id == g->guild_id &&
				!sd->state.waitingdisconnect) {
				g->member[i].sd = sd;
				clif_charnameupdate(sd); // [LuzZza]
			} else g->member[i].sd = NULL;
			m++;
		}else
			g->member[i].sd=NULL;
		if(before.member[i].account_id>0)
			bm++;
	}

	for(i=0;i<g->max_member;i++){	// 情報の送信
		struct map_session_data *sd = g->member[i].sd;
		if( sd==NULL )
			continue;

		if(	before.guild_lv!=g->guild_lv || bm!=m ||
			before.max_member!=g->max_member ){
			clif_guild_basicinfo(sd);	// 基本情報送信
			clif_guild_emblem(sd,g);	// エンブレム送信
		}

		if(bm!=m){		// メンバー情報送信
			clif_guild_memberlist(g->member[i].sd);
		}

		if( before.skill_point!=g->skill_point)
			clif_guild_skillinfo(sd);	// スキル情報送信

		if( sd->state.guild_sent==0){	// 未送信なら所属情報も送る
			clif_guild_belonginfo(sd,g);
			clif_guild_notice(sd,g);
			sd->guild_emblem_id=g->emblem_id;
			sd->state.guild_sent=1;
		}
	}

	// イベントの発生
	if( (ev=idb_remove(guild_infoevent_db,sg->guild_id))!=NULL ){
		while(ev){
			npc_event_do(ev->name);
			ev2=ev->next;
			aFree(ev);
			ev=ev2;
		}
	}

	return 0;
}


// ギルドへの勧誘
int guild_invite(struct map_session_data *sd,struct map_session_data *tsd)
{
	struct guild *g;
	int i;

	nullpo_retr(0, sd);

	g=guild_search(sd->status.guild_id);

	if(tsd==NULL || g==NULL)
		return 0;

	if( (i=guild_getposition(sd,g))<0 || !(g->position[i].mode&0x0001) )
		return 0; //Invite permission.

	if(!battle_config.invite_request_check) {
		if (tsd->party_invite>0 || tsd->trade_partner) {	// 相手が取引中かどうか
			clif_guild_inviteack(sd,0);
			return 0;
		}
	}
	if(tsd->status.guild_id>0 ||
		tsd->guild_invite>0 ||
		(agit_flag && map[tsd->bl.m].flag.gvg_castle))
	{	//Can't invite people inside castles. [Skotlex]
		clif_guild_inviteack(sd,0);
		return 0;
	}

	// 定員確認
	for(i=0;i<g->max_member;i++)
		if(g->member[i].account_id==0)
			break;
	if(i==g->max_member){
		clif_guild_inviteack(sd,3);
		return 0;
	}

	tsd->guild_invite=sd->status.guild_id;
	tsd->guild_invite_account=sd->status.account_id;

	clif_guild_invite(tsd,g);
	return 0;
}
// ギルド勧誘への返答
int guild_reply_invite(struct map_session_data *sd,int guild_id,int flag)
{
	struct map_session_data *tsd;

	nullpo_retr(0, sd);
	
	//nullpo_retr(0, tsd= map_id2sd( sd->guild_invite_account ));
	//I checked the code, and there's no "check" for the case where the guy
	//that invites another to a guild quits the map-server before being replied.
	//Hence that's a valid null pointer scenario. :) [Skotlex]
	if ((tsd= map_id2sd( sd->guild_invite_account )) == NULL)
	{	//Do we send a "invitation failed" msg or something to the player?
		//Or should we accept the invitation and add it to the guild anyway?
		//afterall, guild_invite holds the guild id that the player was invited to.
		sd->guild_invite=0;
		sd->guild_invite_account=0;
		return 0;
	}

	if(sd->guild_invite!=guild_id)	// 勧誘とギルドIDが違う
		return 0;

	if(flag==1){	// 承諾
		struct guild_member m;
		struct guild *g;
		int i;

		// 定員確認
		if( (g=guild_search(tsd->status.guild_id))==NULL ){
			sd->guild_invite=0;
			sd->guild_invite_account=0;
			return 0;
		}
		for(i=0;i<g->max_member;i++)
			if(g->member[i].account_id==0)
				break;
		if(i==g->max_member){
			sd->guild_invite=0;
			sd->guild_invite_account=0;
			clif_guild_inviteack(tsd,3);
			return 0;
		}

		//inter鯖へ追加要求
		guild_makemember(&m,sd);
		intif_guild_addmember( sd->guild_invite, &m );
		//TODO: send a minimap update to this player
		return 0;
	}else{		// 拒否
		sd->guild_invite=0;
		sd->guild_invite_account=0;
		clif_guild_inviteack(tsd,1);
	}
	return 0;
}
// ギルドメンバが追加された
int guild_member_added(int guild_id,int account_id,int char_id,int flag)
{
	struct map_session_data *sd= map_id2sd(account_id),*sd2;
	struct guild *g;

	if( (g=guild_search(guild_id))==NULL )
		return 0;

	if(sd==NULL || sd->guild_invite==0){
		// キャラ側に登録できなかったため脱退要求を出す
		if (flag == 0) {
			if(battle_config.error_log)
				ShowError("guild: member added error %d is not online\n",account_id);
 			intif_guild_leave(guild_id,account_id,char_id,0,"**登録失敗**");
		}
		return 0;
	}
	sd2 = map_id2sd(sd->guild_invite_account);
	sd->guild_invite = 0;
	sd->guild_invite_account = 0;

	if(flag==1){	// 失敗
		if( sd2!=NULL )
			clif_guild_inviteack(sd2,3);
		return 0;
	}

		// 成功
	sd->state.guild_sent = 0;
	sd->status.guild_id = g->guild_id;
	sd->guild_emblem_id = g->emblem_id;
	//TODO: send new emblem info to others

	if( sd2!=NULL )
		clif_guild_inviteack(sd2,2);

	// いちおう競合確認
	guild_check_conflict(sd);
	//Next line commented because it do nothing, look at guild_recv_info [LuzZza]
	//clif_charnameupdate(sd); //Update display name [Skotlex]
	return 0;
}

// ギルド脱退要求
int guild_leave(struct map_session_data* sd, int guild_id, int account_id, int char_id, const char* mes)
{
	struct guild *g;

	nullpo_retr(0, sd);

	g = guild_search(sd->status.guild_id);

	if(g==NULL)
		return 0;

	if(sd->status.account_id!=account_id ||
		sd->status.char_id!=char_id || sd->status.guild_id!=guild_id ||
		(agit_flag && map[sd->bl.m].flag.gvg_castle))
		return 0;

	intif_guild_leave(sd->status.guild_id, sd->status.account_id, sd->status.char_id,0,mes);
	return 0;
}

// ギルド追放要求
int guild_expulsion(struct map_session_data* sd, int guild_id, int account_id, int char_id, const char* mes)
{
	struct map_session_data *tsd;
	struct guild *g;
	int i,ps;

	nullpo_retr(0, sd);

	g = guild_search(sd->status.guild_id);

	if(g==NULL)
		return 0;

	if(sd->status.guild_id!=guild_id)
		return 0;

	if( (ps=guild_getposition(sd,g))<0 || !(g->position[ps].mode&0x0010) )
		return 0;	//Expulsion permission

  	//Can't leave inside guild castles.
	if ((tsd = map_id2sd(account_id)) &&
		tsd->status.char_id == char_id &&
		(agit_flag && map[tsd->bl.m].flag.gvg_castle))
		return 0;

	for(i=0;i<g->max_member;i++){	// 所属しているか
		if(g->member[i].account_id==account_id &&
			g->member[i].char_id==char_id ){
			if(!strcmp(g->member[i].name,g->master))
				return 0; //Can't expel the GM!
			intif_guild_leave(g->guild_id,account_id,char_id,1,mes);
			return 0;
		}
	}
	return 0;
}

int guild_member_leaved(int guild_id, int account_id, int char_id, int flag, const char* name, const char* mes)
{
	int i;
	struct guild* g = guild_search(guild_id);
	struct map_session_data* sd = map_charid2sd(char_id);
	struct map_session_data* online_member_sd;

	if(g == NULL)
		return 0; // no such guild (error!)
	
	for(i = 0; i < g->max_member; i++)
		if( g->member[i].account_id == account_id && g->member[i].char_id == char_id )
			break;

	if (i == g->max_member)
		return 0; // not a member (inconsistency!)

	online_member_sd = guild_getavailablesd(g);
	if(online_member_sd == NULL)
		return 0; // noone online to inform

	if(!flag)
		clif_guild_leave(online_member_sd, name, mes);
	else
		clif_guild_expulsion(online_member_sd, name, mes, account_id);

	// remove member from guild
	memset(&g->member[i],0,sizeof(struct guild_member));
	clif_guild_memberlist(online_member_sd);

	// update char, if online
	if(sd != NULL && sd->status.guild_id == guild_id)
	{
		// do stuff that needs the guild_id first, BEFORE we wipe it
		if (sd->state.storage_flag == 2) //Close the guild storage.
			storage_guild_storageclose(sd);
		guild_send_dot_remove(sd);

		sd->status.guild_id = 0;
		sd->guild_emblem_id = 0;
		sd->state.guild_sent = 0;
		
		clif_charnameupdate(sd); //Update display name [Skotlex]
		//TODO: send emblem update to self and people around
	}
	return 0;
}

int guild_send_memberinfoshort(struct map_session_data *sd,int online)
{ // cleaned up [LuzZza]
	struct guild *g;
	
	nullpo_retr(0, sd);
		
	if(!(g = guild_search(sd->status.guild_id)))
		return 0;

	//Moved to place before intif_guild_memberinfoshort because
	//If it's not a member, needn't send it's info to intif. [LuzZza]
	guild_check_member(g);
	
	if(sd->status.guild_id <= 0)
		return 0;

	intif_guild_memberinfoshort(g->guild_id,
		sd->status.account_id,sd->status.char_id,online,sd->status.base_level,sd->status.class_);

	if(!online){
		int i=guild_getindex(g,sd->status.account_id,sd->status.char_id);
		if(i>=0)
			g->member[i].sd=NULL;
		return 0;
	}
	
	if(sd->state.guild_sent)
		return 0;

	clif_guild_belonginfo(sd,g);
	clif_guild_notice(sd,g);
	
	sd->state.guild_sent = 1;
	sd->guild_emblem_id = g->emblem_id;
	
	return 0;
}

int guild_recv_memberinfoshort(int guild_id,int account_id,int char_id,int online,int lv,int class_)
{ // cleaned up [LuzZza]
	
	int i,alv,c,idx=-1,om=0,oldonline=-1;
	struct guild *g = guild_search(guild_id);
	
	if(g == NULL)
		return 0;
	
	for(i=0,alv=0,c=0,om=0;i<g->max_member;i++){
		struct guild_member *m=&g->member[i];
		if(m->account_id==account_id && m->char_id==char_id ){
			oldonline=m->online;
			m->online=online;
			m->lv=lv;
			m->class_=class_;
			idx=i;
		}
		if(m->account_id>0){
			alv+=m->lv;
			c++;
		}
		if(m->online)
			om++;
	}
	
	if(idx == -1 || c == 0) {
		// ギルドのメンバー外なので追放扱いする
		struct map_session_data *sd = map_id2sd(account_id);
		if(sd && sd->status.char_id == char_id) {
			sd->status.guild_id=0;
			sd->guild_emblem_id=0;
			sd->state.guild_sent=0;
		}
		if(battle_config.error_log)
			ShowWarning("guild: not found member %d,%d on %d[%s]\n",	account_id,char_id,guild_id,g->name);
		return 0;
	}
	
	g->average_lv=alv/c;
	g->connect_member=om;

	for(i=0;i<g->max_member;i++) {
		struct map_session_data *sd= map_id2sd(g->member[i].account_id);
		g->member[i].sd = (sd && sd->status.char_id == g->member[i].char_id &&
			sd->status.guild_id == g->guild_id && !sd->state.waitingdisconnect) ? sd : NULL;
	}
	
	if(oldonline!=online) 
		clif_guild_memberlogin_notice(g, idx, online);
	

	if(!g->member[idx].sd)
		return 0;

	//Send XY dot updates. [Skotlex]
	//Moved from guild_send_memberinfoshort [LuzZza]
	for(i=0; i < g->max_member; i++) {
		
		if(!g->member[i].sd || i == idx ||
			g->member[i].sd->bl.m != g->member[idx].sd->bl.m)
			continue;

		clif_guild_xy_single(g->member[idx].sd->fd, g->member[i].sd);
		clif_guild_xy_single(g->member[i].sd->fd, g->member[idx].sd);
	}			

	return 0;
}
// ギルド会話送信
int guild_send_message(struct map_session_data *sd,char *mes,int len)
{
	nullpo_retr(0, sd);

	if(sd->status.guild_id==0)
		return 0;
	intif_guild_message(sd->status.guild_id,sd->status.account_id,mes,len);
	guild_recv_message(sd->status.guild_id,sd->status.account_id,mes,len);

	// Chat logging type 'G' / Guild Chat
	if( log_config.chat&8 && !(agit_flag && log_config.chat&32) )
		log_chat("G", sd->status.guild_id, sd->status.char_id, sd->status.account_id, mapindex_id2name(sd->mapindex), sd->bl.x, sd->bl.y, NULL, mes);

	return 0;
}
// ギルド会話受信
int guild_recv_message(int guild_id,int account_id,char *mes,int len)
{
	struct guild *g;
	if( (g=guild_search(guild_id))==NULL)
		return 0;
	clif_guild_message(g,account_id,mes,len);
	return 0;
}
// ギルドメンバの役職変更
int guild_change_memberposition(int guild_id,int account_id,int char_id,int idx)
{
	return intif_guild_change_memberinfo(
		guild_id,account_id,char_id,GMI_POSITION,&idx,sizeof(idx));
}
// ギルドメンバの役職変更通知
int guild_memberposition_changed(struct guild *g,int idx,int pos)
{
	nullpo_retr(0, g);

	g->member[idx].position=pos;
	clif_guild_memberpositionchanged(g,idx);
	
	// Update char position in client [LuzZza]
	if(g->member[idx].sd != NULL)
		clif_charnameupdate(g->member[idx].sd);
	return 0;
}
// ギルド役職変更
int guild_change_position(int guild_id,int idx,
	int mode,int exp_mode,const char *name)
{
	struct guild_position p;

	exp_mode = cap_value(exp_mode, 0, battle_config.guild_exp_limit);
	//Mode 0x01 <- Invite
	//Mode 0x10 <- Expel.
	p.mode=mode&0x11;
	p.exp_mode=exp_mode;
	memcpy(p.name,name,NAME_LENGTH);
	p.name[NAME_LENGTH-1] = '\0'; //Security check... [Skotlex]
	return intif_guild_position(guild_id,idx,&p);
}
// ギルド役職変更通知
int guild_position_changed(int guild_id,int idx,struct guild_position *p)
{
	struct guild *g=guild_search(guild_id);
	int i;
	if(g==NULL)
		return 0;
	memcpy(&g->position[idx],p,sizeof(struct guild_position));
	clif_guild_positionchanged(g,idx);
	
	// Update char name in client [LuzZza]
	for(i=0;i<g->max_member;i++)
		if(g->member[i].position == idx && g->member[i].sd != NULL)
			clif_charnameupdate(g->member[i].sd);
	return 0;
}
// ギルド告知変更
int guild_change_notice(struct map_session_data *sd,int guild_id,const char *mes1,const char *mes2)
{
	nullpo_retr(0, sd);

	if(guild_id!=sd->status.guild_id)
		return 0;
	return intif_guild_notice(guild_id,mes1,mes2);
}
// ギルド告知変更通知
int guild_notice_changed(int guild_id,const char *mes1,const char *mes2)
{
	int i;
	struct map_session_data *sd;
	struct guild *g=guild_search(guild_id);
	if(g==NULL)
		return 0;

	memcpy(g->mes1,mes1,60);
	memcpy(g->mes2,mes2,120);

	for(i=0;i<g->max_member;i++){
		if((sd=g->member[i].sd)!=NULL)
			clif_guild_notice(sd,g);
	}
	return 0;
}
// ギルドエンブレム変更
int guild_change_emblem(struct map_session_data *sd,int len,const char *data)
{
	struct guild *g;
	nullpo_retr(0, sd);

	if (battle_config.require_glory_guild &&
		!((g = guild_search(sd->status.guild_id)) && guild_checkskill(g, GD_GLORYGUILD)>0)) {
		clif_skill_fail(sd,GD_GLORYGUILD,0,0);
		return 0;
	}

	return intif_guild_emblem(sd->status.guild_id,len,data);
}
// ギルドエンブレム変更通知
int guild_emblem_changed(int len,int guild_id,int emblem_id,const char *data)
{
	int i;
	struct map_session_data *sd;
	struct guild *g=guild_search(guild_id);
	if(g==NULL)
		return 0;

	memcpy(g->emblem_data,data,len);
	g->emblem_len=len;
	g->emblem_id=emblem_id;

	for(i=0;i<g->max_member;i++){
		if((sd=g->member[i].sd)!=NULL){
			sd->guild_emblem_id=emblem_id;
			clif_guild_belonginfo(sd,g);
			clif_guild_emblem(sd,g);
		}
	}
	return 0;
}

static void* create_expcache(DBKey key, va_list args)
{
	struct guild_expcache *c;
	struct map_session_data *sd = va_arg(args, struct map_session_data*);

	c = ers_alloc(expcache_ers, struct guild_expcache);
	c->guild_id = sd->status.guild_id;
	c->account_id = sd->status.account_id;
	c->char_id = sd->status.char_id;
	c->exp = 0;
	return c;
}

// ギルドのEXP上納
unsigned int guild_payexp(struct map_session_data *sd,unsigned int exp)
{
	struct guild *g;
	struct guild_expcache *c;
	int per;
	
	nullpo_retr(0, sd);

	if (!exp) return 0;
	
	if (sd->status.guild_id == 0 ||
		(g = guild_search(sd->status.guild_id)) == NULL ||
		(per = guild_getposition(sd,g)) < 0 ||
		(per = g->position[per].exp_mode) < 1)
		return 0;
	

	if (per < 100)
		exp = (unsigned int) exp * per / 100;
	//Otherwise tax everything.
	
	c = guild_expcache_db->ensure(guild_expcache_db, i2key(sd->status.char_id), create_expcache, sd);

	if (c->exp > UINT_MAX - exp)
		c->exp = UINT_MAX;
	else
		c->exp += exp;
	
	return exp;
}

// Celest
int guild_getexp(struct map_session_data *sd,int exp)
{
	struct guild *g;
	struct guild_expcache *c;
	nullpo_retr(0, sd);

	if (sd->status.guild_id == 0 || (g = guild_search(sd->status.guild_id)) == NULL)
		return 0;

	c = guild_expcache_db->ensure(guild_expcache_db, i2key(sd->status.char_id), create_expcache, sd);
	if (c->exp > UINT_MAX - exp)
		c->exp = UINT_MAX;
	else
		c->exp += exp;
	return exp;
}

// スキルポイント割り振り
int guild_skillup(TBL_PC* sd, int skill_num)
{
	struct guild* g;
	int idx = skill_num - GD_SKILLBASE;

	nullpo_retr(0, sd);

	if( idx < 0 || idx >= MAX_GUILDSKILL || // not a guild skill
			sd->status.guild_id == 0 || (g=guild_search(sd->status.guild_id)) == NULL || // no guild
			strcmp(sd->status.name, g->master) ) // not the guild master
		return 0;

	if( g->skill_point > 0 &&
			g->skill[idx].id != 0 &&
			g->skill[idx].lv < guild_skill_get_max(skill_num) )
		intif_guild_skillup(g->guild_id, skill_num, sd->status.account_id);

	return 0;
}
// スキルポイント割り振り通知
int guild_skillupack(int guild_id,int skill_num,int account_id)
{
	struct map_session_data *sd=map_id2sd(account_id);
	struct guild *g=guild_search(guild_id);
	int i;
	if(g==NULL)
		return 0;
	if(sd!=NULL)
		clif_guild_skillup(sd,skill_num,g->skill[skill_num-GD_SKILLBASE].lv);
	// 全員に通知
	for(i=0;i<g->max_member;i++)
		if((sd=g->member[i].sd)!=NULL)
			clif_guild_skillinfo(sd);
	return 0;
}

// ギルド同盟数所得
int guild_get_alliance_count(struct guild *g,int flag)
{
	int i,c;

	nullpo_retr(0, g);

	for(i=c=0;i<MAX_GUILDALLIANCE;i++){
		if(	g->alliance[i].guild_id>0 &&
			g->alliance[i].opposition==flag )
			c++;
	}
	return c;
}

// Blocks all guild skills which have a common delay time.
void guild_block_skill(struct map_session_data *sd, int time)
{
	int skill_num[] = { GD_BATTLEORDER, GD_REGENERATION, GD_RESTORE, GD_EMERGENCYCALL };
	int i;
	for (i = 0; i < 4; i++)
		skill_blockpc_start(sd, skill_num[i], time);
}

// 同盟関係かどうかチェック
// 同盟なら1、それ以外は0
int guild_check_alliance(int guild_id1, int guild_id2, int flag)
{
	struct guild *g;
	int i;

	g = guild_search(guild_id1);
	if (g == NULL)
		return 0;

	for (i=0; i<MAX_GUILDALLIANCE; i++)
		if ((g->alliance[i].guild_id == guild_id2) && (g->alliance[i].opposition == flag))
			return 1;

	return 0;
}
// ギルド同盟要求
int guild_reqalliance(struct map_session_data *sd,struct map_session_data *tsd)
{
	struct guild *g[2];
	int i;

	if(agit_flag)	{	// Disable alliance creation during woe [Valaris]
		clif_displaymessage(sd->fd,"Alliances cannot be made during Guild Wars!");
		return 0;
	}	// end addition [Valaris]


	nullpo_retr(0, sd);

	if(tsd==NULL || tsd->status.guild_id<=0)
		return 0;

	g[0]=guild_search(sd->status.guild_id);
	g[1]=guild_search(tsd->status.guild_id);

	if(g[0]==NULL || g[1]==NULL)
		return 0;

	// Prevent creation alliance with same guilds [LuzZza]
	if(sd->status.guild_id == tsd->status.guild_id)
		return 0;

	if( guild_get_alliance_count(g[0],0)>=3 )	{
		clif_guild_allianceack(sd,4);
		return 0;
	}
	if( guild_get_alliance_count(g[1],0)>=3 ) {
		clif_guild_allianceack(sd,3);
		return 0;
	}

	if( tsd->guild_alliance>0 ){
		clif_guild_allianceack(sd,1);
		return 0;
	}

	for(i=0;i<MAX_GUILDALLIANCE;i++){	// すでに同盟状態か確認
		if(	g[0]->alliance[i].guild_id==tsd->status.guild_id &&
			g[0]->alliance[i].opposition==0){
			clif_guild_allianceack(sd,0);
			return 0;
		}
	}

	tsd->guild_alliance=sd->status.guild_id;
	tsd->guild_alliance_account=sd->status.account_id;

	clif_guild_reqalliance(tsd,sd->status.account_id,g[0]->name);
	return 0;
}
// ギルド勧誘への返答
int guild_reply_reqalliance(struct map_session_data *sd,int account_id,int flag)
{
	struct map_session_data *tsd;

	nullpo_retr(0, sd);
	tsd= map_id2sd( account_id );
	if (!tsd) { //Character left? Cancel alliance.
		clif_guild_allianceack(sd,3);
		return 0;
	}

	if(sd->guild_alliance!=tsd->status.guild_id)	// 勧誘とギルドIDが違う
		return 0;

	if(flag==1){	// 承諾
		int i;

		struct guild *g,*tg;	// 同盟数再確認
		g=guild_search(sd->status.guild_id);
		tg=guild_search(tsd->status.guild_id);
		
		if(g==NULL || guild_get_alliance_count(g,0)>=3){
			clif_guild_allianceack(sd,4);
			clif_guild_allianceack(tsd,3);
			return 0;
		}
		if(tg==NULL || guild_get_alliance_count(tg,0)>=3){
			clif_guild_allianceack(sd,3);
			clif_guild_allianceack(tsd,4);
			return 0;
		}

		for(i=0;i<MAX_GUILDALLIANCE;i++){
			if(g->alliance[i].guild_id==tsd->status.guild_id &&
				g->alliance[i].opposition==1)
				intif_guild_alliance( sd->status.guild_id,tsd->status.guild_id,
					sd->status.account_id,tsd->status.account_id,9 );
		}
		for(i=0;i<MAX_GUILDALLIANCE;i++){
			if(tg->alliance[i].guild_id==sd->status.guild_id &&
				tg->alliance[i].opposition==1)
				intif_guild_alliance( tsd->status.guild_id,sd->status.guild_id,
					tsd->status.account_id,sd->status.account_id,9 );
		}

		// inter鯖へ同盟要請
		intif_guild_alliance( sd->status.guild_id,tsd->status.guild_id,
			sd->status.account_id,tsd->status.account_id,0 );
		return 0;
	}else{		// 拒否
		sd->guild_alliance=0;
		sd->guild_alliance_account=0;
		if(tsd!=NULL)
			clif_guild_allianceack(tsd,3);
	}
	return 0;
}
// ギルド関係解消
int guild_delalliance(struct map_session_data *sd,int guild_id,int flag)
{
	nullpo_retr(0, sd);

	if(agit_flag)	{	// Disable alliance breaking during woe [Valaris]
		clif_displaymessage(sd->fd,"Alliances cannot be broken during Guild Wars!");
		return 0;
	}	// end addition [Valaris]

	intif_guild_alliance( sd->status.guild_id,guild_id,
		sd->status.account_id,0,flag|8 );
	return 0;
}
// ギルド敵対
int guild_opposition(struct map_session_data *sd,struct map_session_data *tsd)
{
	struct guild *g;
	int i;

	nullpo_retr(0, sd);

	g=guild_search(sd->status.guild_id);
	if(g==NULL || tsd==NULL)
		return 0;

	// Prevent creation opposition with same guilds [LuzZza]
	if(sd->status.guild_id == tsd->status.guild_id)
		return 0;

	if( guild_get_alliance_count(g,1)>=3 )	{
		clif_guild_oppositionack(sd,1);
		return 0;
	}

	for(i=0;i<MAX_GUILDALLIANCE;i++){	// すでに関係を持っているか確認
		if(g->alliance[i].guild_id==tsd->status.guild_id){
			if(g->alliance[i].opposition==1){	// すでに敵対
				clif_guild_oppositionack(sd,2);
				return 0;
			}
			//Change alliance to opposition.
			intif_guild_alliance( sd->status.guild_id,tsd->status.guild_id,
				sd->status.account_id,tsd->status.account_id,8 );
		}
	}

	// inter鯖に敵対要請
	intif_guild_alliance( sd->status.guild_id,tsd->status.guild_id,
			sd->status.account_id,tsd->status.account_id,1 );
	return 0;
}
// ギルド同盟/敵対通知
int guild_allianceack(int guild_id1,int guild_id2,int account_id1,int account_id2,
	int flag,const char *name1,const char *name2)
{
	struct guild *g[2];
	int guild_id[2];
	const char *guild_name[2];
	struct map_session_data *sd[2];
	int j,i;

	guild_id[0] = guild_id1;
	guild_id[1] = guild_id2;
	guild_name[0] = name1;
	guild_name[1] = name2;
	sd[0] = map_id2sd(account_id1);
	sd[1] = map_id2sd(account_id2);

	g[0]=guild_search(guild_id1);
	g[1]=guild_search(guild_id2);

	if(sd[0]!=NULL && (flag&0x0f)==0){
		sd[0]->guild_alliance=0;
		sd[0]->guild_alliance_account=0;
	}

	if(flag&0x70){	// 失敗
		for(i=0;i<2-(flag&1);i++)
			if( sd[i]!=NULL )
				clif_guild_allianceack(sd[i],((flag>>4)==i+1)?3:4);
		return 0;
	}
//	if(battle_config.etc_log)
//		printf("guild alliance_ack %d %d %d %d %d %s %s\n",guild_id1,guild_id2,account_id1,account_id2,flag,name1,name2);

	if(!(flag&0x08)){	// 関係追加
		for(i=0;i<2-(flag&1);i++)
			if(g[i]!=NULL)
				for(j=0;j<MAX_GUILDALLIANCE;j++)
					if(g[i]->alliance[j].guild_id==0){
						g[i]->alliance[j].guild_id=guild_id[1-i];
						memcpy(g[i]->alliance[j].name,guild_name[1-i],NAME_LENGTH);
						g[i]->alliance[j].opposition=flag&1;
						break;
					}
	}else{				// 関係解消
		for(i=0;i<2-(flag&1);i++){
			if(g[i]!=NULL)
				for(j=0;j<MAX_GUILDALLIANCE;j++)
					if(	g[i]->alliance[j].guild_id==guild_id[1-i] &&
						g[i]->alliance[j].opposition==(flag&1)){
						g[i]->alliance[j].guild_id=0;
						break;
					}
			if( sd[i]!=NULL )	// 解消通知
				clif_guild_delalliance(sd[i],guild_id[1-i],(flag&1));
		}
	}

	if((flag&0x0f)==0){			// 同盟通知
		if( sd[1]!=NULL )
			clif_guild_allianceack(sd[1],2);
	}else if((flag&0x0f)==1){	// 敵対通知
		if( sd[0]!=NULL )
			clif_guild_oppositionack(sd[0],0);
	}


	for(i=0;i<2-(flag&1);i++){	// 同盟/敵対リストの再送信
		struct map_session_data *sd;
		if(g[i]!=NULL)
			for(j=0;j<g[i]->max_member;j++)
				if((sd=g[i]->member[j].sd)!=NULL)
					clif_guild_allianceinfo(sd);
	}
	return 0;
}
// ギルド解散通知用
int guild_broken_sub(DBKey key,void *data,va_list ap)
{
	struct guild *g=(struct guild *)data;
	int guild_id=va_arg(ap,int);
	int i,j;
	struct map_session_data *sd=NULL;

	nullpo_retr(0, g);

	for(i=0;i<MAX_GUILDALLIANCE;i++){	// 関係を破棄
		if(g->alliance[i].guild_id==guild_id){
			for(j=0;j<g->max_member;j++)
				if( (sd=g->member[j].sd)!=NULL )
					clif_guild_delalliance(sd,guild_id,g->alliance[i].opposition);
			intif_guild_alliance(g->guild_id, guild_id,0,0,g->alliance[i].opposition|8);
			g->alliance[i].guild_id=0;
		}
	}
	return 0;
}

//Invoked on Castles when a guild is broken. [Skotlex]
int castle_guild_broken_sub(DBKey key,void *data,va_list ap)
{
	struct guild_castle *gc=(struct guild_castle *)data;
	int guild_id=va_arg(ap,int);

	nullpo_retr(0, gc);

	if (gc->guild_id == guild_id)
	{	//Save the new 'owner', this should invoke guardian clean up and other such things.
		gc->guild_id = 0;
		guild_castledatasave(gc->castle_id, 1, 0);
	}
	return 0;
}

//Innvoked on /breakguild "Guild name"
int guild_broken(int guild_id,int flag)
{
	struct guild *g=guild_search(guild_id);
//	struct guild_castle *gc=NULL;
	struct map_session_data *sd;
	int i;
//	char *name;;

	if(flag!=0 || g==NULL)
		return 0;

	//we call castle_event::OnGuildBreak of all castlesof the guild
	//you can set all castle_events in the castle_db.txt
/*	name=(char *)aCalloc(50,sizeof(char)); //24 char = event name, + space for  "::OnGuildBreak"
	for(i=0;i<MAX_GUILDCASTLE;i++){
		if( (gc=guild_castle_search(i)) != NULL ){
			if(gc->guild_id == guild_id){
				memcpy(name,gc->castle_event,50);
				npc_event_do(strcat(name,"::OnGuildBreak"));
			}
		}
	}
	free(name);
*/
	for(i=0;i<g->max_member;i++){	// ギルド解散を通知
		if((sd=g->member[i].sd)!=NULL){
			if(sd->state.storage_flag == 2)
				storage_guild_storage_quit(sd,1);
			sd->status.guild_id=0;
			sd->state.guild_sent=0;
			clif_guild_broken(g->member[i].sd,0);
			clif_charnameupdate(sd); // [LuzZza]
		}
	}

	guild_db->foreach(guild_db,guild_broken_sub,guild_id);
	castle_db->foreach(castle_db,castle_guild_broken_sub,guild_id);
	guild_storage_delete(guild_id);
	idb_remove(guild_db,guild_id);
	return 0;
}

//Changes the Guild Master to the specified player. [Skotlex]
int guild_gm_change(int guild_id, struct map_session_data *sd)
{
	struct guild *g;
	nullpo_retr(0, sd);

	if (sd->status.guild_id != guild_id)
		return 0;
	
	g=guild_search(guild_id);

	nullpo_retr(0, g);

	if (strcmp(g->master, sd->status.name) == 0) //Nothing to change.
		return 0;

	//Notify servers that master has changed.
	intif_guild_change_gm(guild_id, sd->status.name, strlen(sd->status.name));
	return 1;
}

//Notification from Char server that a guild's master has changed. [Skotlex]
int guild_gm_changed(int guild_id, int account_id, int char_id)
{
	struct guild *g;
	struct guild_member gm;
	int pos;

	g=guild_search(guild_id);

	if (!g)
		return 0;

	for(pos=0; pos<g->max_member && !(
		g->member[pos].account_id==account_id &&
		g->member[pos].char_id==char_id);
		pos++);

	if (pos == 0 || pos == g->max_member) return 0;

	memcpy(&gm, &g->member[pos], sizeof (struct guild_member));
	memcpy(&g->member[pos], &g->member[0], sizeof(struct guild_member));
	memcpy(&g->member[0], &gm, sizeof(struct guild_member));

	g->member[pos].position = g->member[0].position;
	g->member[0].position = 0; //Position 0: guild Master.
	strcpy(g->master, g->member[0].name);

	if (g->member[pos].sd && g->member[pos].sd->fd)
	{
		clif_displaymessage(g->member[pos].sd->fd, "You no longer are the Guild Master.");
		g->member[pos].sd->state.gmaster_flag = 0;
	}
	
	if (g->member[0].sd && g->member[0].sd->fd)
	{
		clif_displaymessage(g->member[0].sd->fd, "You have become the Guild Master!");
		g->member[0].sd->state.gmaster_flag = g;
		//Block his skills for 5 minutes to prevent abuse.
		guild_block_skill(g->member[0].sd, 300000);
	}	
	return 1;
}

// ギルド解散
int guild_break(struct map_session_data *sd,char *name)
{
	struct guild *g;
	int i;

	nullpo_retr(0, sd);

	if( (g=guild_search(sd->status.guild_id))==NULL )
		return 0;
	if(strcmp(g->name,name)!=0)
		return 0;
	if(!sd->state.gmaster_flag)
		return 0;
	for(i=0;i<g->max_member;i++){
		if(	g->member[i].account_id>0 && (
			g->member[i].account_id!=sd->status.account_id ||
			g->member[i].char_id!=sd->status.char_id ))
			break;
	}
	if(i<g->max_member){
		clif_guild_broken(sd,2);
		return 0;
	}

	intif_guild_break(g->guild_id);
	return 0;
}

// ギルド城データ要求
int guild_castledataload(int castle_id,int index)
{
	return intif_guild_castle_dataload(castle_id,index);
}
// ギルド城情報所得時イベント追加
int guild_addcastleinfoevent(int castle_id,int index,const char *name)
{
	struct eventlist *ev;
	int code=castle_id|(index<<16);

	if( name==NULL || *name==0 )
		return 0;

	ev=(struct eventlist *)aMalloc(sizeof(struct eventlist));
	memcpy(ev->name,name,sizeof(ev->name));
	//The next event becomes whatever was currently stored.
	ev->next= idb_put(guild_castleinfoevent_db,code,ev);
	return 0;
}

// ギルド城データ要求返信
int guild_castledataloadack(int castle_id,int index,int value)
{
	struct guild_castle *gc=guild_castle_search(castle_id);
	int code=castle_id|(index<<16);
	struct eventlist *ev,*ev2;

	if(gc==NULL){
		return 0;
	}
	switch(index){
	case 1:
		gc->guild_id = value;
		if (value && guild_search(value)==NULL) //Request guild data which will be required for spawned guardians. [Skotlex]
			guild_request_info(value);
		break;
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
	default:
		ShowError("guild_castledataloadack ERROR!! (Not found index=%d)\n", index);
		return 0;
	}
	if( (ev=idb_remove(guild_castleinfoevent_db,code))!=NULL ){
		while(ev){
			npc_event_do(ev->name);
			ev2=ev->next;
			aFree(ev);
			ev=ev2;
		}
	}
	return 1;
}
// ギルド城データ変更要求
int guild_castledatasave(int castle_id,int index,int value)
{
	if (index == 1)
	{	//The castle's owner has changed? Update Guardian ownership, too. [Skotlex]
		struct guild_castle *gc = guild_castle_search(castle_id);
		int m = -1;
		if (gc) m = map_mapname2mapid(gc->map_name);
		if (m != -1)
			map_foreachinmap(mob_guardian_guildchange, m, BL_MOB);
	}
	return intif_guild_castle_datasave(castle_id,index,value);
}

// ギルド城データ変更通知
int guild_castledatasaveack(int castle_id,int index,int value)
{
	struct guild_castle *gc=guild_castle_search(castle_id);
	if(gc==NULL){
		return 0;
	}
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
	default:
		ShowError("guild_castledatasaveack ERROR!! (Not found index=%d)\n", index);
		return 0;
	}
	return 1;
}

// ギルドデータ一括受信（初期化時）
int guild_castlealldataload(int len,struct guild_castle *gc)
{
	int i;
	int n = (len-4) / sizeof(struct guild_castle), ev = -1;

	nullpo_retr(0, gc);

	//Last owned castle in the list invokes ::OnAgitinit
	for(i = 0; i < n; i++) {
		if ((gc + i)->guild_id)
			ev = i;
	}

	// 城データ格納とギルド情報要求
	for(i = 0; i < n; i++, gc++) {
		struct guild_castle *c = guild_castle_search(gc->castle_id);
		if (!c) {
			ShowError("guild_castlealldataload Castle id=%d not found.\n", gc->castle_id);
			continue;
		}
		memcpy(&c->guild_id,&gc->guild_id,
			sizeof(struct guild_castle) - ((int)&c->guild_id - (int)c) );
		if( c->guild_id ){
			if(i!=ev)
				guild_request_info(c->guild_id);
			else
				guild_npc_request_info(c->guild_id, "::OnAgitInit");
		}
	}
	if (ev == -1) //No castles owned, invoke OnAgitInit as it is.
		npc_event_doall("OnAgitInit");
	return 0;
}

int guild_agit_start(void)
{	// Run All NPC_Event[OnAgitStart]
	int c = npc_event_doall("OnAgitStart");
	ShowStatus("NPC_Event:[OnAgitStart] Run (%d) Events by @AgitStart.\n",c);
	// Start auto saving
	guild_save_timer = add_timer_interval (gettick() + GUILD_SAVE_INTERVAL, guild_save_sub, 0, 0, GUILD_SAVE_INTERVAL);
	return 0;
}

int guild_agit_end(void)
{	// Run All NPC_Event[OnAgitEnd]
	int c = npc_event_doall("OnAgitEnd");
	ShowStatus("NPC_Event:[OnAgitEnd] Run (%d) Events by @AgitEnd.\n",c);
	// Stop auto saving
	delete_timer (guild_save_timer, guild_save_sub);
	return 0;
}

int guild_gvg_eliminate_timer(int tid,unsigned int tick,int id,int data)
{	// Run One NPC_Event[OnAgitEliminate]
	char *name = (char*)data;
	size_t len = (name) ? strlen(name) : 0; 
	// the rest is dangerous, but let it crash,
	// if this happens, it's ruined anyway
	int c=0;

	if(agit_flag)	// Agit not already End
	{
		char *evname=(char*)aMalloc( (len + 10) * sizeof(char));
		memcpy(evname,name,len - 5);
		strcpy(evname + len - 5,"Eliminate");
		c = npc_event_do(evname);
		ShowStatus("NPC_Event:[%s] Run (%d) Events.\n",evname,c);
		aFree(evname); // [Lance] Should fix this
	}
	if(name) aFree(name);
	return 0;
}

static int Ghp[MAX_GUILDCASTLE][MAX_GUARDIANS];	// so save only if HP are changed // experimental code [Yor]
static int Gid[MAX_GUILDCASTLE];
int guild_save_sub(int tid,unsigned int tick,int id,int data)
{
	struct guild_castle *gc;
	int i,j;

	for(i = 0; i < MAX_GUILDCASTLE; i++) {	// [Yor]
		gc = guild_castle_search(i);
		if (!gc) continue;
		if (gc->guild_id != Gid[i]) {
			// Re-save guild id if its owner guild has changed
			guild_castledatasave(gc->castle_id, 1, gc->guild_id);
			Gid[i] = gc->guild_id;
		}
		for (j = 0; j < MAX_GUARDIANS; j++)
		{
			if (gc->guardian[j].visible && Ghp[i][j] != gc->guardian[j].hp)
				guild_castledatasave(gc->castle_id, 18+j, gc->guardian[j].hp);
		}
	}

	return 0;
}

int guild_agit_break(struct mob_data *md)
{	// Run One NPC_Event[OnAgitBreak]
	char *evname;

	if(!agit_flag) return 0;	// Agit already End
	evname=(char *)aMallocA((strlen(md->npc_event) + 1)*sizeof(char));

	strcpy(evname,md->npc_event);
// Now By User to Run [OnAgitBreak] NPC Event...
// It's a little impossible to null point with player disconnect in this!
// But Script will be stop, so nothing...
// Maybe will be changed in the futher..
//      int c = npc_event_do(evname);
	add_timer(gettick()+battle_config.gvg_eliminate_time,guild_gvg_eliminate_timer,md->bl.m,(int)evname);
	return 0;
}

// How many castles does this guild have?
int guild_checkcastles(struct guild *g)
{
	int i, nb_cas = 0;
	struct guild_castle* gc;

	for(i = 0; i < MAX_GUILDCASTLE; i++) {
		gc = guild_castle_search(i);
		if(gc && gc->guild_id == g->guild_id)
			nb_cas++;
	}

	return nb_cas;
}

// [MouseJstr]
//    is this guild allied with this castle?
int guild_isallied(struct guild *g, struct guild_castle *gc)
{
	int i;

	nullpo_retr(0, g);

	if(g->guild_id == gc->guild_id)
		return 1;

	if (gc->guild_id == 0)
		return 0;


	for(i=0;i<MAX_GUILDALLIANCE;i++)
		if(g->alliance[i].guild_id == gc->guild_id) {
			if(g->alliance[i].opposition == 0)
				return 1;
			else
				return 0;
		}

	return 0;
}

int guild_idisallied(int guild_id, int guild_id2)
{
	int i;
	struct guild *g;

	if (guild_id <= 0 || guild_id2 <= 0)
		return 0;
	
	if(guild_id == guild_id2)
		return 1;

	g = guild_search(guild_id);

	nullpo_retr(0, g);


	for(i=0;i<MAX_GUILDALLIANCE;i++)
		if(g->alliance[i].guild_id == guild_id2) {
			if(g->alliance[i].opposition == 0)
				return 1;
			else
				return 0;
		}

	return 0;
}

static int guild_infoevent_db_final(DBKey key,void *data,va_list ap)
{
	aFree(data);
	return 0;
}

static int guild_expcache_db_final(DBKey key,void *data,va_list ap)
{
	ers_free(expcache_ers, data);
	return 0;
}

void do_final_guild(void)
{
	guild_db->destroy(guild_db,NULL);
	castle_db->destroy(castle_db,NULL);
	guild_expcache_db->destroy(guild_expcache_db,guild_expcache_db_final);
	guild_infoevent_db->destroy(guild_infoevent_db,guild_infoevent_db_final);
	guild_castleinfoevent_db->destroy(guild_castleinfoevent_db,guild_infoevent_db_final);
	ers_destroy(expcache_ers);
}
