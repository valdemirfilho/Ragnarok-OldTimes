// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef _MAP_H_
#define _MAP_H_

#include <stdarg.h>
#include "../common/mmo.h"

#define MAX_PC_CLASS 4046 //original code: all classes (4050) by Mehah
#define PC_CLASS_BASE 0
#define PC_CLASS_BASE2 (PC_CLASS_BASE + 4001)
#define PC_CLASS_BASE3 (PC_CLASS_BASE2 + 22)
#define MAX_NPC_PER_MAP 512
#define BLOCK_SIZE 8
#define AREA_SIZE battle_config.area_size
#define LOCAL_REG_NUM 16
#define LIFETIME_FLOORITEM 60
#define DAMAGELOG_SIZE 30
#define LOOTITEM_SIZE 10
#define MAX_STATUSCHANGE 250
//Quick defines to know which are the min-max common ailments. [Skotlex]
//Because of the way the headers are included.. these must be replaced for actual values.
//Remember to update as needed! Min is SC_STONE and max is SC_DPOISON currently.
#define SC_COMMON_MIN 0
#define SC_COMMON_MAX 10

#define MAX_SKILL_LEVEL 100
#define MAX_SKILLUNITGROUP 32
#define MAX_MOBSKILLUNITGROUP 8
#define MAX_SKILLUNITGROUPTICKSET 32
#define MAX_SKILLTIMERSKILL 32
#define MAX_MOBSKILLTIMERSKILL 10
#define MAX_MOBSKILL 32
#define MAX_MOB_LIST_PER_MAP 128
#define MAX_EVENTQUEUE 2
#define MAX_EVENTTIMER 32
#define NATURAL_HEAL_INTERVAL 500
#define MAX_FLOORITEM 500000
#define MAX_LEVEL 255
#define MAX_WALKPATH 32
#define MAX_DROP_PER_MAP 48
#define MAX_IGNORE_LIST 80
#define MAX_VENDING 12
#define MOBID_EMPERIUM 1288

#define MAX_PC_BONUS 10
#define MAX_DUEL 1024

//These mark the ID of the jobs, as expected by the client. [Skotlex]
enum {
	JOB_NOVICE,
	JOB_SWORDMAN,
	JOB_MAGE,
	JOB_ARCHER,
	JOB_ACOLYTE,
	JOB_MERCHANT,
	JOB_THIEF,
	JOB_KNIGHT,
	JOB_PRIEST,
	JOB_WIZARD,
	JOB_BLACKSMITH,
	JOB_HUNTER,
	JOB_ASSASSIN,
	JOB_KNIGHT2,
	JOB_CRUSADER,
	JOB_MONK,
	JOB_SAGE,
	JOB_ROGUE,
	JOB_ALCHEMIST,
	JOB_BARD,
	JOB_DANCER,
	JOB_CRUSADER2,
	JOB_WEDDING,
	JOB_SUPER_NOVICE,
	JOB_GUNSLINGER,
	JOB_NINJA,
	JOB_XMAS,

	JOB_NOVICE_HIGH = 4001,
	JOB_SWORDMAN_HIGH,
	JOB_MAGE_HIGH,
	JOB_ARCHER_HIGH,
	JOB_ACOLYTE_HIGH,
	JOB_MERCHANT_HIGH,
	JOB_THIEF_HIGH,
	JOB_LORD_KNIGHT,
	JOB_HIGH_PRIEST,
	JOB_HIGH_WIZARD,
	JOB_WHITESMITH,
	JOB_SNIPER,
	JOB_ASSASSIN_CROSS,
	JOB_LORD_KNIGHT2,
	JOB_PALADIN,
	JOB_CHAMPION,
	JOB_PROFESSOR,
	JOB_STALKER,
	JOB_CREATOR,
	JOB_CLOWN,
	JOB_GYPSY,
	JOB_PALADIN2,

	JOB_BABY,
	JOB_BABY_SWORDMAN,
	JOB_BABY_MAGE,
	JOB_BABY_ARCHER,
	JOB_BABY_ACOLYTE,
	JOB_BABY_MERCHANT,
	JOB_BABY_THIEF,
	JOB_BABY_KNIGHT,
	JOB_BABY_PRIEST,
	JOB_BABY_WIZARD,
	JOB_BABY_BLACKSMITH,
	JOB_BABY_HUNTER,
	JOB_BABY_ASSASSIN,
	JOB_BABY_KNIGHT2,
	JOB_BABY_CRUSADER,
	JOB_BABY_MONK,
	JOB_BABY_SAGE,
	JOB_BABY_ROGUE,
	JOB_BABY_ALCHEMIST,
	JOB_BABY_BARD,
	JOB_BABY_DANCER,
	JOB_BABY_CRUSADER2,
	JOB_SUPER_BABY,

	JOB_TAEKWON,
	JOB_STAR_GLADIATOR,
	JOB_STAR_GLADIATOR2,
	JOB_SOUL_LINKER,
};

//The following system marks a different job ID system used by the map server,
//which makes a lot more sense than the normal one. [Skotlex]
//
//These marks the "level" of the job.
#define JOBL_2_1 0x100 //256
#define JOBL_2_2 0x200 //512
#define JOBL_2 0x300

#define JOBL_UPPER 0x1000 //4096
#define JOBL_BABY 0x2000  //8192

//for filtering and quick checking.
#define MAPID_UPPERMASK 0x0fff
#define MAPID_BASEMASK 0x00ff
//First Jobs
//Note the oddity of the novice:
//Super Novices are considered the 2-1 version of the novice! Novices are considered a first class type, too...
enum {
	MAPID_NOVICE = 0x0,
	MAPID_SWORDMAN,
	MAPID_MAGE,
	MAPID_ARCHER,
	MAPID_ACOLYTE,
	MAPID_MERCHANT,
	MAPID_THIEF,
	MAPID_TAEKWON,
	MAPID_WEDDING,
	MAPID_XMAS, // [Valaris]
//2_1 classes
	MAPID_SUPER_NOVICE = JOBL_2_1|0x0,
	MAPID_KNIGHT,
	MAPID_WIZARD,
	MAPID_HUNTER,
	MAPID_PRIEST,
	MAPID_BLACKSMITH,
	MAPID_ASSASSIN,
	MAPID_STAR_GLADIATOR,
//2_2 classes
	MAPID_CRUSADER = JOBL_2_2|0x1,
	MAPID_SAGE,
	MAPID_BARDDANCER,
	MAPID_MONK,
	MAPID_ALCHEMIST,
	MAPID_ROGUE,
	MAPID_SOUL_LINKER,
//1-1, advanced
	MAPID_NOVICE_HIGH = JOBL_UPPER|0x0,
	MAPID_SWORDMAN_HIGH,
	MAPID_MAGE_HIGH,
	MAPID_ARCHER_HIGH,
	MAPID_ACOLYTE_HIGH,
	MAPID_MERCHANT_HIGH,
	MAPID_THIEF_HIGH,
//2_1 advanced
	MAPID_LORD_KNIGHT = JOBL_UPPER|JOBL_2_1|0x1,
	MAPID_HIGH_WIZARD,
	MAPID_SNIPER,
	MAPID_HIGH_PRIEST,
	MAPID_WHITESMITH,
	MAPID_ASSASSIN_CROSS,
//2_2 advanced
	MAPID_PALADIN = JOBL_UPPER|JOBL_2_2|0x1,
	MAPID_PROFESSOR,
	MAPID_CLOWNGYPSY,
	MAPID_CHAMPION,
	MAPID_CREATOR,
	MAPID_STALKER,
//1-1 baby
	MAPID_BABY = JOBL_BABY|0x0,
	MAPID_BABY_SWORDMAN,
	MAPID_BABY_MAGE,
	MAPID_BABY_ARCHER,
	MAPID_BABY_ACOLYTE,
	MAPID_BABY_MERCHANT,
	MAPID_BABY_THIEF,
	MAPID_BABY_TAEKWON,
//2_1 baby
	MAPID_SUPER_BABY = JOBL_BABY|JOBL_2_1|0x0,
	MAPID_BABY_KNIGHT,
	MAPID_BABY_WIZARD,
	MAPID_BABY_HUNTER,
	MAPID_BABY_PRIEST,
	MAPID_BABY_BLACKSMITH,
	MAPID_BABY_ASSASSIN,
	MAPID_BABY_STAR_GLADIATOR,
//2_2 baby
	MAPID_BABY_CRUSADER = JOBL_BABY|JOBL_2_2|0x1,
	MAPID_BABY_SAGE,
	MAPID_BABY_BARDDANCER,
	MAPID_BABY_MONK,
	MAPID_BABY_ALCHEMIST,
	MAPID_BABY_ROGUE,
	MAPID_BABY_SOUL_LINKER,
};

//Don't change this, as the client seems to always send/receive 80 characters as it currently is. [Skotlex]
#define MESSAGE_SIZE 80

#define DEFAULT_AUTOSAVE_INTERVAL 60*1000

#define OPTION_SIGHT 0x0001
#define OPTION_HIDE 0x0002
#define OPTION_CLOAK 0x0004

#define OPTION_FALCON 0x0010
#define OPTION_RIDING 0x0020
#define OPTION_INVISIBLE 0x0040
#define OPTION_ORCISH 0x0800

#define OPTION_WEDDING 0x1000
#define OPTION_RUWACH 0x2000
#define OPTION_CHASEWALK 0x4000

#define OPTION_FLYING 0x8000

//TODO: Get these Missing options...
#define OPTION_SIGHTTRASHER 0x0001

//Specifies maps where players may hit each other
#define map_flag_vs(m) (map[m].flag.pvp || map[m].flag.gvg_dungeon || map[m].flag.gvg || (agit_flag && map[m].flag.gvg_castle))
//Specifies maps that have special GvG/WoE restrictions
#define map_flag_gvg(m) (map[m].flag.gvg || (agit_flag && map[m].flag.gvg_castle))

enum { BL_NUL, BL_PC, BL_NPC, BL_MOB, BL_ITEM, BL_CHAT, BL_SKILL , BL_PET };
enum { WARP, SHOP, SCRIPT, MONS };

struct block_list {
	struct block_list *next,*prev;
	int id;
	short m,x,y;
	unsigned char type;
	unsigned char subtype;
};

struct walkpath_data {
	unsigned char path_len,path_pos,path_half;
	unsigned char path[MAX_WALKPATH];
};
struct shootpath_data {
	int rx,ry,len;
	int x[MAX_WALKPATH];
	int y[MAX_WALKPATH];
};

struct script_reg {
	int index;
	int data;
};
struct script_regstr {
	int index;
	char data[256];
};
struct status_change {
	int timer;
	int val1,val2,val3,val4;
};
struct vending {
	short index;
	unsigned short amount;
	unsigned int value;
};

struct weapon_data {
 	int atkmods[3];
 	// all the variables except atkmods get zero'ed in each call of status_calc_pc
	// NOTE: if you want to add a non-zeroed variable, you need to update the memset call
	//  in status_calc_pc as well! All the following are automatically zero'ed. [Skotlex]
  	int watk;
  	int watk2;
 	int atk_ele;
  	int overrefine;
  	int star;
  	int ignore_def_ele;
  	int ignore_def_race;
  	int def_ratio_atk_ele;
  	int def_ratio_atk_race;
 	int addele[10];
 	int addrace[12];
 	int addrace2[12];
 	int addsize[3];

  	short ignore_def_mob;
  	short hp_drain_rate;
  	short hp_drain_per;
  	short hp_drain_value;
  	short sp_drain_rate;
  	short sp_drain_per;
  	short sp_drain_value;
 	short add_damage_classid[MAX_PC_BONUS];
 	int add_damage_classrate[MAX_PC_BONUS];
  	int add_damage_class_count;
};

struct skill_unit_group;
struct skill_unit {
	struct block_list bl;

	struct skill_unit_group *group;

	int limit;
	int val1,val2;
	short alive,range;
};
struct skill_unit_group {
	int src_id;
	int party_id;
	int guild_id;
	int map;
	int target_flag;
	unsigned int tick;
	int limit,interval;

	int skill_id,skill_lv;
	int val1,val2,val3;
	char *valstr;
	int unit_id;
	int group_id;
	int unit_count,alive_count;
	struct skill_unit *unit;
};
struct skill_unit_group_tickset {
	unsigned int tick;
	int id;
};
struct skill_timerskill {
	int timer;
	int src_id;
	int target_id;
	int map;
	short x,y;
	short skill_id,skill_lv;
	int type;
	int flag;
};

struct npc_data;
struct pet_db;
struct item_data;
struct square;

struct map_session_data {
	struct block_list bl;
	//NOTE: When deciding to add a flag to state or special_state, take into consideration that state is preserved in
	//status_calc_pc, while special_state is recalculated in each call. [Skotlex]
	struct {
		unsigned auth : 1;
		unsigned change_walk_target : 1;
		unsigned attack_continue : 1;
		unsigned menu_or_input : 1;
		unsigned dead_sit : 2;
		unsigned skillcastcancel : 1;
		unsigned waitingdisconnect : 1;
		unsigned lr_flag : 2;
		unsigned connect_new : 1;
		unsigned arrow_atk : 1;
		unsigned attack_type : 3;
		unsigned skill_flag : 1;
		unsigned gangsterparadise : 1;
		unsigned rest : 1;
		unsigned produce_flag : 1;
		unsigned storage_flag : 2; //0: closed, 1: Normal Storage open, 2: guild storage open [Skotlex]
		unsigned snovice_flag : 4;
		struct guild *gmaster_flag;
		// originally by Qamera, adapted by celest
		unsigned event_death : 1;
		unsigned event_kill : 1;
		unsigned event_disconnect : 1;
		// Abracadabra bugfix by Aru
		unsigned abra_flag : 1;
		unsigned autoloot : 1; //by Upa-Kun <- Ya know, I originally wrote this code... [Skotlex]
		unsigned autotrade : 1;	//By Fantik
		unsigned perfect_hiding : 1; // [Valaris]
		unsigned accreg_dirty : 1; //By Skotlex (marks whether Account variables have been saved or not yet)
		unsigned showdelay :1;
		unsigned showexp :1;
		unsigned showzeny :1;
		unsigned mainchat :1; //[LuzZza] 
		unsigned disguised :1; //[Valaris]
		unsigned deal_locked :2;
		unsigned party_sent :1;
		unsigned guild_sent :1;
		unsigned monster_ignore :1; // for monsters to ignore a character [Valaris] [zzo]
		unsigned size :2; // for tiny/large types
		unsigned night :1; //Holds whether or not the player currently has the SI_NIGHT effect on. [Skotlex]
	} state;
	struct {
		unsigned killer : 1;
		unsigned killable : 1;
		unsigned restart_full_recover : 1;
		unsigned no_castcancel : 1;
		unsigned no_castcancel2 : 1;
		unsigned no_sizefix : 1;
		unsigned no_magic_damage : 1;
		unsigned no_weapon_damage : 1;
		unsigned no_gemstone : 1;
		unsigned infinite_endure : 1;
		unsigned intravision : 1; // Maya Purple Card effect allowing to see Hiding/Cloaking people [DracoRPG]
	} special_state;
	int char_id, login_id1, login_id2, sex;
	unsigned short class_;	//This is the internal job ID used by the map server to simplify comparisons/queries/etc. [Skotlex]

	int packet_ver;  // 5: old, 6: 7july04, 7: 13july04, 8: 26july04, 9: 9aug04/16aug04/17aug04, 10: 6sept04, 11: 21sept04, 12: 18oct04, 13: 25oct04 ... 18
	struct mmo_charstatus status;
	struct item_data *inventory_data[MAX_INVENTORY];
	short equip_index[11];
	unsigned int weight,max_weight;
	int cart_weight,cart_max_weight,cart_num,cart_max_num;
	char mapname[MAP_NAME_LENGTH];
	int fd;
	short to_x,to_y;
	short speed,prev_speed;
	short opt1,opt2,opt3;
	unsigned char dir,head_dir;
	unsigned int client_tick,server_tick;
	struct walkpath_data walkpath;
	int walktimer;
	int npc_id,areanpc_id,npc_shopid;
	int npc_item_flag; //Marks the npc_id with which you can use items during interactions with said npc (see script command enable_itemuse)
	int npc_pos;
	int npc_menu;
	int npc_amount;
	struct script_stack *stack;
	unsigned char *npc_script,*npc_scriptroot;
	int  npc_scriptstate;
	char npc_str[256];
	unsigned int chatID;
	time_t idletime;

	struct{
		char name[NAME_LENGTH];
	} ignore[MAX_IGNORE_LIST];
	int ignoreAll;

	int attacktimer;

	int attacktarget;
	short attacktarget_lv;
	unsigned int attackabletime;

	int followtimer; // [MouseJstr]
	int followtarget;

	time_t emotionlasttime; // to limit flood with emotion packets

	int skilltimer;
	int skilltarget;
	short skillx,skilly;
	short skillid,skilllv;
	short skillitem,skillitemlv;
	short skillid_old,skilllv_old;
	short skillid_dance,skilllv_dance;
	struct skill_unit_group skillunit[MAX_SKILLUNITGROUP];
	struct skill_unit_group_tickset skillunittick[MAX_SKILLUNITGROUPTICKSET];
	struct skill_timerskill skilltimerskill[MAX_SKILLTIMERSKILL];
	char blockskill[MAX_SKILL];	// [celest]
	//unsigned int skillstatictimer[MAX_SKILL];
	unsigned short timerskill_count; // [celest]
	int cloneskill_id;
	struct map_session_data *repair_target;

	int invincible_timer;
	unsigned int canact_tick;
	unsigned int canmove_tick;
	unsigned int canlog_tick;
	unsigned int canregen_tick;
	unsigned int canuseitem_tick;	// [Skotlex]
	int hp_sub,sp_sub;
	int inchealhptick,inchealsptick,inchealspirithptick,inchealspiritsptick;

	short view_class;
	short weapontype1,weapontype2;
	short disguise; // [Valaris]

	struct weapon_data right_weapon;
	struct weapon_data left_weapon;
	
 	int paramc[6],paramcard[6];
 
 	// here start arrays to be globally zeroed at the beginning of status_calc_pc()
 
 	int paramb[6];
 	int parame[6];
 	int subele[10];
 	int subrace[12];
  	int subrace2[12];
  	int subsize[3];
 	int addeff[SC_COMMON_MAX-SC_COMMON_MIN+1];
 	int addeff2[SC_COMMON_MAX-SC_COMMON_MIN+1];
 	int reseff[SC_COMMON_MAX-SC_COMMON_MIN+1];
 	int weapon_coma_ele[10];
 	int weapon_coma_race[12];
 	int weapon_atk[16];
 	int weapon_atk_rate[16];
 	int arrow_addele[10];
 	int arrow_addrace[12];
 	int arrow_addsize[3];
 	int arrow_addeff[SC_COMMON_MAX-SC_COMMON_MIN+1];
 	int arrow_addeff2[SC_COMMON_MAX-SC_COMMON_MIN+1];
 	int magic_addele[10];
 	int magic_addrace[12];
 	int magic_addsize[3];
  	int critaddrace[12];
 	int expaddrace[12];
 	int itemhealrate[7];
  	int addeff3[SC_COMMON_MAX-SC_COMMON_MIN+1];
  	short addeff3_type[SC_COMMON_MAX-SC_COMMON_MIN+1];
  	short sp_gain_race[12];
  	short unequip_losehp[11];
  	short unequip_losesp[11];
 	// zeroed arrays end here.
	// zeroed structures start here
	struct {
		short id, lv, rate;
	} autospell[MAX_PC_BONUS], autospell2[MAX_PC_BONUS];
	struct { //skillatk raises bonus dmg% of skills, skillblown increases bonus blewcount for some skills.
		short id, val;
	} skillatk[MAX_PC_BONUS], skillblown[MAX_PC_BONUS];
	struct {
		short class_, rate;
	}	add_def[MAX_PC_BONUS], add_mdef[MAX_PC_BONUS],
		add_dmg[MAX_PC_BONUS], add_mdmg[MAX_PC_BONUS];
	struct { 
		short id, group;
		int race, rate;
	} add_drop[MAX_PC_BONUS];
	// zeroed structures end here
 	// zeroed vars start here.
 	int hit;
 	int flee, flee2;
 	int critical;
 	int aspd;
 	int def, def2;
 	int mdef, mdef2;
 	int def_ele;
 	int matk1, matk2;
 	int base_atk;
 	int arrow_atk,arrow_ele,arrow_cri,arrow_hit,arrow_range;
 	int nhealhp,nhealsp,nshealhp,nshealsp,nsshealhp,nsshealsp;
 	int critical_def,double_rate;
 	int long_attack_atk_rate; //Long range atk rate, not weapon based. [Skotlex]
 	int near_attack_def_rate,long_attack_def_rate,magic_def_rate,misc_def_rate;
 	int ignore_mdef_ele;
 	int ignore_mdef_race;
 	int perfect_hit;
 	int perfect_hit_add;
 	int get_zeny_rate;
 	int get_zeny_num; //Added Get Zeny Rate [Skotlex]
 	int double_add_rate;
 	int short_weapon_damage_return,long_weapon_damage_return;
  	int magic_damage_return; // AppleGirl Was Here
  	int random_attack_increase_add,random_attack_increase_per; // [Valaris]
 	int break_weapon_rate,break_armor_rate;
 	int crit_atk_rate;
 	int hp_loss_rate;
 	int sp_loss_rate;
  	int classchange; // [Valaris]
 	unsigned int setitem_hash, setitem_hash2; //Split in 2 because shift operations only work on int ranges. [Skotlex]
 
 	short attackrange,attackrange_;
 	short splash_range, splash_add_range;
 	short add_steal_rate;
 	short hp_loss_value;
 	short sp_loss_value;
 	short hp_loss_type;
 	short sp_drain_type;
 	short sp_gain_value, hp_gain_value;
	short add_drop_count;
	unsigned short unbreakable;	// chance to prevent ANY equipment breaking [celest]
 	unsigned short unbreakable_equip; //100% break resistance on certain equipment
 	unsigned short unstripable_equip;
 	short no_regen;
 	short add_def_count,add_mdef_count;
 	short add_dmg_count,add_mdmg_count;
 
 	// zeroed vars end here.
 
 	int amotion,dmotion;
 	int castrate,delayrate,hprate,sprate,dsprate;
 	int atk_rate;
 	int aspd_rate,speed_rate,hprecov_rate,sprecov_rate;
 	int matk_rate;
 	int critical_rate,hit_rate,flee_rate,flee2_rate,def_rate,def2_rate,mdef_rate,mdef2_rate;
 	int speed_add_rate, aspd_add_rate;
 
 	int hp_loss_tick;
 	int sp_loss_tick;
 
 	int itemid;
 	short itemindex;	//Used item's index in sd->inventory [Skotlex]
 
	short catch_target_class; // pet catching, stores a pet class to catch (short now) [zzo]

 	short spiritball, spiritball_old;
 	int spirit_timer[MAX_SKILL_LEVEL];
  
  	int die_counter;
  	short doridori_counter;
	char potion_success_counter;

	int reg_num;
	struct script_reg *reg;
	int regstr_num;
	struct script_regstr *regstr;

	struct status_change sc_data[MAX_STATUSCHANGE];
	short sc_count;
	short mission_mobid; //Stores the target mob_id for TK_MISSION
	short mission_count; //Stores the bounty kill count for TK_MISSION
	int devotion[5]; //Stores the char IDs of chars devoted to.
	
	int trade_partner;
	struct { 
		struct {
			int index, amount;
		} item[10];
		int zeny, weight;
	} deal;

	int party_invite,party_invite_account;
	short party_x,party_y; // should be short [zzo]

	int guild_invite,guild_invite_account;
	int guild_emblem_id,guild_alliance,guild_alliance_account;
	short guild_x,guild_y; // For guildmate position display. [Skotlex] should be short [zzo]
	int guildspy; // [Syrus22]
	int partyspy; // [Syrus22]

	int vender_id;
	int vend_num;
	char message[MESSAGE_SIZE];
	struct vending vending[MAX_VENDING];

	struct s_pet pet;
	struct pet_db *petDB;
	struct pet_data *pd;
	int pet_hungry_timer;

	struct{
		int  m; //-1 - none, other: map index corresponding to map name.
		char name[MAP_NAME_LENGTH]; //map name
	}feel_map[3];// 0 - Sun; 1 - Moon; 2 - Stars
	int feel_level;
	short hate_mob[3];

	unsigned int pvp_timer;
	short pvp_point;
	unsigned short pvp_rank, pvp_lastusers;
	unsigned short pvp_won, pvp_lost;

	char eventqueue[MAX_EVENTQUEUE][50];
	int eventtimer[MAX_EVENTTIMER];
	unsigned short eventcount; // [celest]

	unsigned char change_level; // [celest]

	char fakename[NAME_LENGTH]; // fake names [Valaris]

#ifndef TXT_ONLY
	int mail_counter;	// mail counter for mail system [Valaris]
#endif

	int duel_group; // duel vars [LuzZza]
    int duel_invite;
    
    char away_message[128]; // [LuzZza]
    
};

struct {
	int members_count;
	int invites_count;
	int max_players_limit;
} duel_list[MAX_DUEL];

int duel_count;

struct npc_timerevent_list {
	int timer,pos;
};
struct npc_label_list {
	char name[NAME_LENGTH];
	int pos;
};
struct npc_item_list {
	unsigned int nameid,value;
};
struct npc_data {
	struct block_list bl;
	short n;
	short class_,dir;
	short speed;
	char name[NAME_LENGTH];
	char exname[NAME_LENGTH];
	int chat_id;
	short opt1,opt2,opt3,option;
	short flag;
	int walktimer; // [Valaris]
	short to_x,to_y; // [Valaris]
	struct walkpath_data walkpath;
	unsigned int next_walktime;
	unsigned int canmove_tick;

	struct { // [Valaris]
		unsigned state : 8;
		unsigned change_walk_target : 1;
		unsigned walk_easy : 1;
	} state;

	char eventqueue[MAX_EVENTQUEUE][50];
	int eventtimer[MAX_EVENTTIMER];
	short arenaflag;

	void *chatdb;

	union {
		struct {
			unsigned char *script;
			short xs,ys;
			int guild_id;
			int timer,timerid,timeramount,nexttimer,rid;
			unsigned int timertick;
			struct npc_timerevent_list *timer_event;
			int label_list_num;
			struct npc_label_list *label_list;
			int src_id;
		} scr;
		struct npc_item_list shop_item[1];
		struct {
			short xs,ys;
			short x,y;
			char name[MAP_NAME_LENGTH];
		} warp;
	} u;
	//Do NOT place anything afterwards... shop data NPC will override any variables from here and on! [Skotlex]
};

//For quick linking to a guardian's info. [Skotlex]
struct guardian_data {
	int number; //0-MAX_GUARDIANS-1 = Guardians. MAX_GUARDIANS = Emperium.
	int guild_id;
	int emblem_id;
	int guardup_lv; //Level of GD_GUARDUP skill.
	char guild_name[NAME_LENGTH];
	struct guild_castle* castle;
};

struct mob_data {
	struct block_list bl;
	struct mob_db *db;	//For quick data access (saves doing mob_db(md->class_) all the time) [Skotlex]
	char name[NAME_LENGTH];
	struct {
		unsigned size : 2; //Small/Big monsters.
		unsigned cached : 1; //Cached mobs for dynamic mob unloading [Skotlex]
		unsigned ai : 3; //Special ai for summoned monsters.
	} special_state; //Special mob information that does not needs to be zero'ed on mob respawn.
	struct {
		unsigned state : 8;
		unsigned skillstate : 8;
		unsigned aggressive : 1; //Signals whether the mob AI is in aggressive mode or reactive mode. [Skotlex]
		unsigned targettype : 1;
		unsigned steal_flag : 1;
		unsigned steal_coin_flag : 1;
		unsigned skillcastcancel : 1;
		unsigned change_walk_target : 1;
		unsigned walk_easy : 1;
		unsigned soul_change_flag : 1; // Celest
		unsigned alchemist: 1;
		int provoke_flag; // Celest
	} state;
	struct status_change sc_data[MAX_STATUSCHANGE];
	struct walkpath_data walkpath;
	struct guardian_data* guardian_data; 
	struct item *lootitem;
	struct {
		int id;
		int dmg;
	} dmglog[DAMAGELOG_SIZE];
	unsigned long tdmg; //Stores total damage given to the mob, for exp calculations. [Skotlex]
	short n;
	short base_class,class_,dir,mode,level;
	short m,x0,y0,xs,ys;
	short to_x,to_y;
	short target_dir;
	short speed;
	short attacked_count;
	short target_lv;
	int timer;
	int hp, max_hp;
	int target_id,attacked_id;
	int spawndelay1,spawndelay2;
	unsigned int attackabletime, canmove_tick, next_walktime;
	unsigned int last_deadtime,last_spawntime,last_thinktime,last_linktime;
	short move_fail_count;
	short lootitem_count;
	short sc_count;
	short opt1,opt2,opt3,option;
	short min_chase;
	
	int deletetimer;
	int skilltimer;
	int skilltarget;
	int def_ele;
	int master_id,master_dist;
	
	short skillx,skilly,skillid,skilllv,skillidx;
	unsigned int skilldelay[MAX_MOBSKILL];
	struct skill_timerskill skilltimerskill[MAX_MOBSKILLTIMERSKILL];
	struct skill_unit_group skillunit[MAX_MOBSKILLUNITGROUP];
	struct skill_unit_group_tickset skillunittick[MAX_SKILLUNITGROUPTICKSET];
	char npc_event[50];
};

struct pet_data {
	struct block_list bl;
	short n;
	short class_,dir;
	struct mob_db *db;
	short speed;
	char name[NAME_LENGTH];
	struct {
		unsigned state : 8 ;
		unsigned skillstate : 8 ;
		unsigned change_walk_target : 1 ;
		unsigned casting_flag :1 ;//Skotlex: Used to identify when we are casting.
		short skillbonus;
	} state;
	int timer;
	short to_x,to_y;
	short equip;
	struct walkpath_data walkpath;
	int target_id;
	short target_lv;
	int move_fail_count;
	unsigned int attackabletime,next_walktime,last_thinktime;
	short rate_fix;	//Support rate as modified by intimacy (1000 = 100%) [Skotlex]
	struct pet_status { //Pet Status data
		short level;
		short atk1,atk2;
		short str,agi,vit,int_,dex,luk;
	} *status;  //[Skotlex]

	struct pet_recovery { //Stat recovery
		unsigned short type;	//Status Change id
		unsigned short delay; //How long before curing (secs).
		int timer;
	} *recovery; //[Valaris] / Reimplemented by [Skotlex]

	struct pet_bonus {
		unsigned short type; //bStr, bVit?
		unsigned short val;	//Qty
		unsigned short duration; //in secs
		unsigned short delay;	//Time before recasting (secs)
		int timer;
	} *bonus; //[Valaris] / Reimplemented by [Skotlex]

	struct pet_skill_attack { //Attack Skill
		unsigned short id;
		unsigned short lv;
		unsigned short div_; //0 = Normal skill. >0 = Fixed damage (lv), fixed div_.
		unsigned short rate; //Base chance of skill ocurrance (10 = 10% of attacks)
		unsigned short bonusrate; //How being 100% loyal affects cast rate (10 = At 1000 intimacy->rate+10%
	} *a_skill;	//[Skotlex]

	struct pet_skill_support { //Support Skill
		unsigned short id;
		unsigned short lv;
		unsigned short hp; //Max HP% for skill to trigger (50 -> 50% for Magnificat)
		unsigned short sp; //Max SP% for skill to trigger (100 = no check)
		unsigned short delay; //Time (secs) between being able to recast.
		int timer;
	} *s_skill;	//[Skotlex]

	struct pet_loot {
		struct item *item;
		unsigned short count;
		unsigned short weight;
		unsigned short max;
		int timer;
	} *loot; //[Valaris] / Rewritten by [Skotlex]

	struct skill_timerskill skilltimerskill[MAX_MOBSKILLTIMERSKILL]; // [Valaris]
	struct skill_unit_group skillunit[MAX_MOBSKILLUNITGROUP]; // [Valaris]
	struct skill_unit_group_tickset skillunittick[MAX_SKILLUNITGROUPTICKSET]; // [Valaris]
	struct map_session_data *msd;
};

enum { MS_IDLE,MS_WALK,MS_ATTACK,MS_DEAD,MS_DELAY };

enum { NONE_ATTACKABLE,ATTACKABLE };

enum { ATK_LUCKY=1,ATK_FLEE,ATK_DEF};	// 囲まれペナルティ計算用

// For equipment breaking/stripping effects
enum {
	EQP_WEAPON		= 1,		// Both weapons
	EQP_ARMOR		= 2,		// Armor
	EQP_SHIELD		= 4,		// Shield
	EQP_HELM		= 8,		// Top-head headgear
};

// Mob List Held in memory for Dynamic Mobs [Wizputer]
struct mob_list {
    int m,x,y,xs,ys,class_,num,delay1,delay2,level;
    char mobname[NAME_LENGTH],eventname[NAME_LENGTH];
};

struct map_data {
	char name[MAP_NAME_LENGTH];
	unsigned char *gat;	// NULLなら下のmap_data_other_serverとして扱う
	unsigned char *cell; //Contains temporary cell data that is set/unset on tiles.
	char *alias; // [MouseJstr]
	struct block_list **block;
	struct block_list **block_mob;
	int *block_count,*block_mob_count;
	int m;
	short xs,ys;
	short bxs,bys;
	int npc_num;
	int users;
	struct {
		unsigned alias : 1;
		unsigned nomemo : 1;
		unsigned noteleport : 1;
		unsigned noreturn : 1;
		unsigned monster_noteleport : 1;
		unsigned nosave : 1;
		unsigned nobranch : 1;
		unsigned nopenalty : 1;
		unsigned pvp : 1;
		unsigned pvp_noparty : 1;
		unsigned pvp_noguild : 1;
		unsigned pvp_nightmaredrop :1;
		unsigned pvp_nocalcrank : 1;
		unsigned gvg_castle : 1;
		unsigned gvg : 1; // Now it identifies gvg versus maps that are active 24/7
		unsigned gvg_dungeon : 1; // Celest
		unsigned gvg_noparty : 1;
		unsigned nozenypenalty : 1;
		unsigned notrade : 1;
		unsigned noskill : 1;
		unsigned nowarp : 1;
		unsigned nowarpto : 1;
		unsigned nopvp : 1; // [Valaris]
		unsigned noicewall : 1; // [Valaris]
		unsigned snow : 1; // [Valaris]
		unsigned clouds : 1;
		unsigned fog : 1; // [Valaris]
		unsigned fireworks : 1;
		unsigned sakura : 1; // [Valaris]
		unsigned leaves : 1; // [Valaris]
		unsigned rain : 1; // [Valaris]
		unsigned indoors : 1; // celest
		unsigned nogo : 1; // [Valaris]
		unsigned nobaseexp	: 1; // [Lorky] added by Lupus
		unsigned nojobexp	: 1; // [Lorky]
		unsigned nomobloot	: 1; // [Lorky]
		unsigned nomvploot	: 1; // [Lorky]
		unsigned nightenabled :1; //For night display. [Skotlex]
	} flag;
	struct point save;
	struct npc_data *npc[MAX_NPC_PER_MAP];
	struct {
		int drop_id;
		int drop_type;
		int drop_per;
	} drop_list[MAX_DROP_PER_MAP];
	struct mob_list *moblist[MAX_MOB_LIST_PER_MAP]; // [Wizputer]
	int mob_delete_timer;	// [Skotlex]
};

struct map_data_other_server {
	char name[NAME_LENGTH];
	unsigned char *gat;	// NULL固定にして判断
	unsigned long ip;
	unsigned int port;
	struct map_data* map;
};

struct flooritem_data {
	struct block_list bl;
	unsigned char subx,suby;
	int cleartimer;
	int first_get_id,second_get_id,third_get_id;
	unsigned int first_get_tick,second_get_tick,third_get_tick;
	struct item item_data;
};

enum {
	SP_SPEED,SP_BASEEXP,SP_JOBEXP,SP_KARMA,SP_MANNER,SP_HP,SP_MAXHP,SP_SP,	// 0-7
	SP_MAXSP,SP_STATUSPOINT,SP_0a,SP_BASELEVEL,SP_SKILLPOINT,SP_STR,SP_AGI,SP_VIT,	// 8-15
	SP_INT,SP_DEX,SP_LUK,SP_CLASS,SP_ZENY,SP_SEX,SP_NEXTBASEEXP,SP_NEXTJOBEXP,	// 16-23
	SP_WEIGHT,SP_MAXWEIGHT,SP_1a,SP_1b,SP_1c,SP_1d,SP_1e,SP_1f,	// 24-31
	SP_USTR,SP_UAGI,SP_UVIT,SP_UINT,SP_UDEX,SP_ULUK,SP_26,SP_27,	// 32-39
	SP_28,SP_ATK1,SP_ATK2,SP_MATK1,SP_MATK2,SP_DEF1,SP_DEF2,SP_MDEF1,	// 40-47
	SP_MDEF2,SP_HIT,SP_FLEE1,SP_FLEE2,SP_CRITICAL,SP_ASPD,SP_36,SP_JOBLEVEL,	// 48-55
	SP_UPPER,SP_PARTNER,SP_CART,SP_FAME,SP_UNBREAKABLE,	//56-60
	SP_CARTINFO=99,	// 99

	SP_BASEJOB=119,	// 100+19 - celest
	SP_BASECLASS=120,	//Hmm.. why 100+19? I just use the next one... [Skotlex]
	
	// original 1000-
	SP_ATTACKRANGE=1000,	SP_ATKELE,SP_DEFELE,	// 1000-1002
	SP_CASTRATE, SP_MAXHPRATE, SP_MAXSPRATE, SP_SPRATE, // 1003-1006
	SP_ADDELE, SP_ADDRACE, SP_ADDSIZE, SP_SUBELE, SP_SUBRACE, // 1007-1011
	SP_ADDEFF, SP_RESEFF,	// 1012-1013
	SP_BASE_ATK,SP_ASPD_RATE,SP_HP_RECOV_RATE,SP_SP_RECOV_RATE,SP_SPEED_RATE, // 1014-1018
	SP_CRITICAL_DEF,SP_NEAR_ATK_DEF,SP_LONG_ATK_DEF, // 1019-1021
	SP_DOUBLE_RATE, SP_DOUBLE_ADD_RATE, SP_MATK, SP_MATK_RATE, // 1022-1025
	SP_IGNORE_DEF_ELE,SP_IGNORE_DEF_RACE, // 1026-1027
	SP_ATK_RATE,SP_SPEED_ADDRATE,SP_ASPD_ADDRATE, // 1028-1030
	SP_MAGIC_ATK_DEF,SP_MISC_ATK_DEF, // 1031-1032
	SP_IGNORE_MDEF_ELE,SP_IGNORE_MDEF_RACE, // 1033-1034
	SP_MAGIC_ADDELE,SP_MAGIC_ADDRACE,SP_MAGIC_ADDSIZE, // 1035-1037
	SP_PERFECT_HIT_RATE,SP_PERFECT_HIT_ADD_RATE,SP_CRITICAL_RATE,SP_GET_ZENY_NUM,SP_ADD_GET_ZENY_NUM, // 1038-1042
	SP_ADD_DAMAGE_CLASS,SP_ADD_MAGIC_DAMAGE_CLASS,SP_ADD_DEF_CLASS,SP_ADD_MDEF_CLASS, // 1043-1046
	SP_ADD_MONSTER_DROP_ITEM,SP_DEF_RATIO_ATK_ELE,SP_DEF_RATIO_ATK_RACE,SP_ADD_SPEED, // 1047-1050
	SP_HIT_RATE,SP_FLEE_RATE,SP_FLEE2_RATE,SP_DEF_RATE,SP_DEF2_RATE,SP_MDEF_RATE,SP_MDEF2_RATE, // 1051-1057
	SP_SPLASH_RANGE,SP_SPLASH_ADD_RANGE,SP_AUTOSPELL,SP_HP_DRAIN_RATE,SP_SP_DRAIN_RATE, // 1058-1062
	SP_SHORT_WEAPON_DAMAGE_RETURN,SP_LONG_WEAPON_DAMAGE_RETURN,SP_WEAPON_COMA_ELE,SP_WEAPON_COMA_RACE, // 1063-1066
	SP_ADDEFF2,SP_BREAK_WEAPON_RATE,SP_BREAK_ARMOR_RATE,SP_ADD_STEAL_RATE, // 1067-1070
	SP_MAGIC_DAMAGE_RETURN,SP_RANDOM_ATTACK_INCREASE,SP_ALL_STATS,SP_AGI_VIT,SP_AGI_DEX_STR,SP_PERFECT_HIDE, // 1071-1076
	SP_DISGUISE,SP_CLASSCHANGE, // 1077-1078
	SP_HP_DRAIN_VALUE,SP_SP_DRAIN_VALUE, // 1079-1080
	SP_WEAPON_ATK,SP_WEAPON_ATK_RATE, // 1081-1082
	SP_DELAYRATE,	// 1083

	SP_RESTART_FULL_RECOVER=2000,SP_NO_CASTCANCEL,SP_NO_SIZEFIX,SP_NO_MAGIC_DAMAGE,SP_NO_WEAPON_DAMAGE,SP_NO_GEMSTONE, // 2000-2005
	SP_NO_CASTCANCEL2,SP_INFINITE_ENDURE,SP_UNBREAKABLE_WEAPON,SP_UNBREAKABLE_ARMOR, SP_UNBREAKABLE_HELM, // 2006-2010
	SP_UNBREAKABLE_SHIELD, SP_LONG_ATK_RATE, // 2011-2012

	SP_CRIT_ATK_RATE, SP_CRITICAL_ADDRACE, SP_NO_REGEN, SP_ADDEFF_WHENHIT, SP_AUTOSPELL_WHENHIT, // 2013-2017
	SP_SKILL_ATK, SP_UNSTRIPABLE, SP_ADD_DAMAGE_BY_CLASS, // 2018-2020
	SP_SP_GAIN_VALUE, SP_IGNORE_DEF_MOB, SP_HP_LOSS_RATE, SP_ADDRACE2, SP_HP_GAIN_VALUE, // 2021-2025
	SP_SUBSIZE, SP_DAMAGE_WHEN_UNEQUIP, SP_ADD_ITEM_HEAL_RATE, SP_LOSESP_WHEN_UNEQUIP, SP_EXP_ADDRACE,	// 2026-2030
	SP_SP_GAIN_RACE, SP_SUBRACE2, SP_ADDEFF_WHENHIT_SHORT,	// 2031-2033
	SP_UNSTRIPABLE_WEAPON,SP_UNSTRIPABLE_ARMOR,SP_UNSTRIPABLE_HELM,SP_UNSTRIPABLE_SHIELD,  // 2034-2037
	SP_INTRAVISION, SP_ADD_MONSTER_DROP_ITEMGROUP, SP_SP_LOSS_RATE, // 2038-2040
	SP_ADD_SKILL_BLOW //2041
};

enum {
	LOOK_BASE,LOOK_HAIR,LOOK_WEAPON,LOOK_HEAD_BOTTOM,LOOK_HEAD_TOP,LOOK_HEAD_MID,LOOK_HAIR_COLOR,LOOK_CLOTHES_COLOR,LOOK_SHIELD,LOOK_SHOES
};

// CELLs for non-permanent cell-based effects (Pneuma, Basilica, Npcs, etc)
#define CELL_NPC	0x1
#define CELL_REGEN	0x2
#define CELL_PNEUMA	0x4
#define CELL_SAFETYWALL	0x8
#define CELL_LANDPROTECTOR	0x10
#define CELL_BASILICA	0x20
#define CELL_MOONLIT	0x40
/*
 * map_getcell()で使用されるフラグ
 */
typedef enum {
	CELL_CHKWALL=0,		// 壁(セルタイプ1)
	CELL_CHKWATER,		// 水場(セルタイプ3)
	CELL_CHKGROUND,		// 地面障害物(セルタイプ5)
	CELL_CHKPASS,		// 通過可能(セルタイプ1,5以外)
	CELL_CHKNOPASS,		// 通過不可(セルタイプ1,5)
	CELL_GETTYPE,		// セルタイプを返す
	CELL_GETCELLTYPE,
	CELL_CHKNPC=0x10,	// タッチタイプのNPC(セルタイプ0x80フラグ)
	CELL_CHKREGEN,		// cells that improve regeneration
	CELL_CHKPNEUMA,
	CELL_CHKSAFETYWALL,
	CELL_CHKBASILICA,	// バジリカ(セルタイプ0x40フラグ)
	CELL_CHKLANDPROTECTOR,
	CELL_CHKMOONLIT,
} cell_t;
// map_setcell()で使用されるフラグ
enum {
	CELL_SETNPC=0x10,	// タッチタイプのNPCをセット
	CELL_SETBASILICA,	// バジリカをセット
	CELL_CLRBASILICA,	// バジリカをクリア
	CELL_SETREGEN,		// set regen cell
	CELL_SETLANDPROTECTOR, //Set/Clear Magnetic Earth
	CELL_CLRLANDPROTECTOR,
	CELL_SETPNEUMA,
	CELL_CLRPNEUMA,
	CELL_SETSAFETYWALL,
	CELL_CLRSAFETYWALL,
	CELL_SETMOONLIT,
	CELL_CLRMOONLIT,
};

struct chat_data {
	struct block_list bl;

	unsigned char pass[8];   /* password */
	unsigned char title[61]; /* room title MAX 60 */
	unsigned char limit;     /* join limit */
	unsigned char trigger;
	unsigned char users;     /* current users */
	unsigned char pub;       /* room attribute */
	struct map_session_data *usersd[20];
	struct block_list *owner_;
	struct block_list **owner;
	char npc_event[50];
};

extern struct map_data map[];
extern int map_num;
extern int autosave_interval;
extern int agit_flag;
extern int night_flag; // 0=day, 1=night [Yor]
extern int kick_on_disconnect; //To allow inter-server reconnections without kicking players out [Skotlex]
extern int enable_spy; //Determines if @spy commands are active.
extern char db_path[256];

// gat?ﾖｧ
int map_getcell(int,int,int,cell_t);
int map_getcellp(struct map_data*,int,int,cell_t);
void map_setcell(int,int,int,int);
extern int map_read_flag; // 0: grfｫﾕｫ｡ｫ､ｫ・1: ｫｭｫ罩ﾃｫｷｫ・2: ｫｭｫ罩ﾃｫｷｫ・?��)
enum {
	READ_FROM_GAT, READ_FROM_AFM,
	READ_FROM_BITMAP, CREATE_BITMAP,
	READ_FROM_BITMAP_COMPRESSED, CREATE_BITMAP_COMPRESSED
};

extern char motd_txt[];
extern char help_txt[];
extern char help2_txt[];
extern char charhelp_txt[];

extern char talkie_mes[];

extern char wisp_server_name[];

// 鯖全体情報
void map_setusers(int);
int map_getusers(void);
// block削除関連
int map_freeblock(struct block_list *bl);
int map_freeblock_lock(void);
int map_freeblock_unlock(void);
// block関連
int map_addblock(struct block_list *);
int map_delblock(struct block_list *);
int map_foreachinarea(int (*)(struct block_list*,va_list),int,int,int,int,int,int,...);
// -- moonsoul (added map_foreachincell)
int map_foreachincell(int (*)(struct block_list*,va_list),int,int,int,int,...);
int map_foreachinmovearea(int (*)(struct block_list*,va_list),int,int,int,int,int,int,int,int,...);
int map_foreachinpath(int (*func)(struct block_list*,va_list),int m,int x0,int y0,int x1,int y1,int range,int type,...); // Celest
int map_countnearpc(int,int,int);
//block関連に追加
int map_count_oncell(int m,int x,int y,int type);
struct skill_unit *map_find_skill_unit_oncell(struct block_list *,int x,int y,int skill_id,struct skill_unit *);
// 一時的object関連
int map_addobject(struct block_list *);
int map_delobject(int);
int map_delobjectnofree(int id);
void map_foreachobject(int (*)(struct block_list*,va_list),int,...);
//
int map_quit(struct map_session_data *);
// npc
int map_addnpc(int,struct npc_data *);

// 床アイテム関連
int map_clearflooritem_timer(int,unsigned int,int,int);
int map_removemobs_timer(int,unsigned int,int,int);
#define map_clearflooritem(id) map_clearflooritem_timer(0,0,id,1)
int map_addflooritem(struct item *,int,int,int,int,struct map_session_data *,struct map_session_data *,struct map_session_data *,int);
int map_searchrandfreecell(int,int,int,int);

// キャラid＝＞キャラ名 変換関連
void map_addchariddb(int charid,char *name);
void map_delchariddb(int charid);
int map_reqchariddb(struct map_session_data * sd,int charid);
char * map_charid2nick(int);
struct map_session_data * map_charid2sd(int);

struct map_session_data * map_id2sd(int);
struct block_list * map_id2bl(int);
int map_mapname2mapid(char*);
int map_mapname2ipport(char*,int*,int*);
int map_setipport(char *name,unsigned long ip,int port);
int map_eraseipport(char *name,unsigned long ip,int port);
int map_eraseallipport(void);
void map_addiddb(struct block_list *);
void map_deliddb(struct block_list *bl);
struct map_session_data** map_getallusers(int *users);
int map_foreachiddb(int (*)(void*,void*,va_list),...);
void map_addnickdb(struct map_session_data *);
struct map_session_data * map_nick2sd(char*);
int compare_item(struct item *a, struct item *b);

// その他
int map_check_dir(int s_dir,int t_dir);
int map_calc_dir( struct block_list *src,int x,int y);
int map_random_dir(struct block_list *bl, short *x, short *y); // [Skotlex]

// Water functions...
// 
int map_setwaterheight(int m, char *mapname, int height);
int map_waterheight(char *mapname);

// path.cより
int path_search(struct walkpath_data*,int,int,int,int,int,int);
int path_search_long(struct shootpath_data *,int,int,int,int,int);
int path_blownpos(int m,int x0,int y0,int dx,int dy,int count);

// distance related functions [Skotlex]
#define check_distance_bl(bl1, bl2, distance) check_distance((bl1)->x - (bl2)->x, (bl1)->y - (bl2)->y, distance)
#define check_distance_blxy(bl, x1, y1, distance) check_distance((bl)->x-(x1), (bl)->y-(y1), distance)
#define check_distance_xy(x0, y0, x1, y1, distance) check_distance((x0)-(x1), (y0)-(y1), distance)
int check_distance(int dx, int dy, int distance);

#define distance_bl(bl1, bl2) distance((bl1)->x - (bl2)->x, (bl1)->y - (bl2)->y)
#define distance_blxy(bl, x1, y1) distance((bl)->x-(x1), (bl)->y-(y1))
#define distance_xy(x0, y0, x1, y1) distance((x0)-(x1), (y0)-(y1))
unsigned int distance(int dx, int dy);

int cleanup_sub(struct block_list *bl, va_list ap);

void map_helpscreen(); // [Valaris]
int map_delmap(char *mapname);

struct mob_list* map_addmobtolist(unsigned short m);	// [Wizputer]
void map_spawnmobs(int); // [Wizputer]
void map_removemobs(int); // [Wizputer]

//Added for own save method
int charsql_db_init(int method);

extern char *INTER_CONF_NAME;
extern char *LOG_CONF_NAME;
extern char *MAP_CONF_NAME;
extern char *BATTLE_CONF_FILENAME;
extern char *ATCOMMAND_CONF_FILENAME;
extern char *CHARCOMMAND_CONF_FILENAME;
extern char *SCRIPT_CONF_NAME;
extern char *MSG_CONF_NAME;
extern char *GRF_PATH_FILENAME;


extern int charsave_method; //needed ..

#ifndef TXT_ONLY

// MySQL
#ifdef __WIN32
#include <my_global.h>
#include <my_sys.h>
#endif
#include <mysql.h>

extern char tmp_sql[65535];

extern int db_use_sqldbs;
extern MYSQL mmysql_handle;
extern MYSQL_RES*	sql_res ;
extern MYSQL_ROW	sql_row ;

extern MYSQL lmysql_handle;
extern MYSQL_RES*	lsql_res ;
extern MYSQL_ROW	lsql_row ;

extern MYSQL charsql_handle;
extern MYSQL_RES*	charsql_res;
extern MYSQL_ROW	charsql_row;

extern MYSQL logmysql_handle;
extern MYSQL_RES*	logsql_res ;
extern MYSQL_ROW	logsql_row ;

extern int mail_server_enable;
extern MYSQL mail_handle;
extern MYSQL_RES* 	mail_res ;
extern MYSQL_ROW	mail_row ;

extern char item_db_db[32];
extern char item_db2_db[32];
extern char mob_db_db[32];
extern char mob_db2_db[32];
extern char login_db[32];

// SQL for databases not supported yet. [Valaris]
extern int db_use_newsqldbs;

extern char abra_sqldb[32];
extern char attr_fix_sqldb[32];
extern char cast_sqldb[32];
extern char castle_sqldb[32];
extern char create_arrow_sqldb[32];
extern char exp_sqldb[32];
extern char exp_guild_sqldb[32];
extern char item_bluebox_sqldb[32];
extern char item_cardalbum_sqldb[32];
extern char item_giftbox_sqldb[32];
extern char item_scroll_sqldb[32];
extern char item_violetbox_sqldb[32];
extern char job_sqldb1[32];
extern char mob_boss_sqldb[32];
extern char mob_branch_sqldb[32];
extern char mob_poring_sqldb[32];
extern char mob_skill_sqldb[32];
extern char pet_sqldb[32];
extern char produce_sqldb[32];
extern char refine_sqldb[32];
extern char size_fix_sqldb[32];
extern char skill_sqldb[32];
extern char skill_require_sqldb[32];
extern char skill_tree_sqldb[32];
// End [Valaris]

extern char login_db_level[32];
extern char login_db_account_id[32];

extern char gm_db[32];
extern char gm_db_level[32];
extern char gm_db_account_id[32];

extern int read_gm_interval;

extern char char_db[32];

#ifdef MAPREGSQL
// [zBuffer] SQL Mapreg
extern MYSQL mapregsql_handle;
extern MYSQL_RES* mapregsql_res ;
extern MYSQL_ROW mapregsql_row;
#endif

extern char mail_db[32];

#endif /* not TXT_ONLY */

extern int lowest_gm_level;

#endif
