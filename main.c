//
//  main.c
//  SMMarble
//
//  Created by Juyeop Kim on 2023/11/05.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "smm_object.h"
#include "smm_database.h"
#include "smm_common.h"

#define BOARDFILEPATH "marbleBoardConfig.txt"
#define FOODFILEPATH "marbleFoodConfig.txt"
#define FESTFILEPATH "marbleFestivalConfig.txt"


//board configuration parameters
static int smm_board_nr;
static int smm_food_nr;
static int smm_festival_nr;
static int smm_player_nr;
static int smm_lab_index;

typedef struct{
	char name[MAX_CHARNAME];
	int pos;
	int credit;
	int energy;
	int flag_graduated;
	int flag_experiment;
	int exp_criterion;
} smm_player_t;

smm_player_t *smm_players;

void generatePlayers(int n, int initEnergy); //generate a new player
void printPlayerStatus(void); //print all player status at the beginning of each turn
void printGrades(int player);

//function prototypes
#if 0
void printGrades(int player); //print grade history of the player
void goForward(int player, int step); //make player go "step" steps on the board (check if player is graduated)
float calcAverageGrade(int player); //calculate average grade of the player
smmGrade_e takeLecture(int player, char *lectureName, int credit); //take the lecture (insert a grade of the player)
void printGrades(int player); //print all the grade history of the player
#endif

void* findGrade(int player, char *lectureName)
{
	int size = smmdb_len(LISTNO_OFFSET_GRADE+player);
	int i;
	
	for (i=0;i<size;i++)
	{
		void *ptr = smmdb_getData(LISTNO_OFFSET_GRADE+player, i);
		if (strcmp(smmObj_getObjectName(ptr), lectureName) == 0)
		{
			return ptr;
		}
	}
	
	return NULL;
}

int isGraduated(void)
{
	int i;
	for (i=0;i<smm_player_nr;i++)
	{
		if (smm_players[i].flag_graduated == 1)
			return 1;
	}
	return 0;
}

void printGrades(int player)
{
	int size = smmdb_len(LISTNO_OFFSET_GRADE+player);
	int i;
	printf("\n[Grades of %s]\n", smm_players[player].name);
	if (size == 0)
	{
		printf(" (empty)\n");
		return;
	}
	for (i=0;i<size;i++)
	{
		void *ptr = smmdb_getData(LISTNO_OFFSET_GRADE+player, i);
		printf("  %s  credit:%d  grade:%s\n", smmObj_getObjectName(ptr), smmObj_getObjectCredit(ptr), smmObj_getGradeName(smmObj_getObjectGrade(ptr)));
	}
}

static int findLaboratoryIndex(void)
{
	int i;
	for (i=0;i<smm_board_nr;i++)
	{
		void *ptr = smmdb_getData(LISTNO_NODE, i);
		if (smmObj_getNodeType(ptr) == SMMNODE_TYPE_LABORATORY)
			return i;
	}
	return -1;
}

void goForward(int player, int step)
{ //make player go "step" steps on the board (check if player is graduated)
	int i;
	void *ptr;
	int homeEnergy = smmObj_getNodeEnergy(smmdb_getData(LISTNO_NODE, 0));
	
	//player_pos[player] = player_pos[player] + step;
	ptr = smmdb_getData(LISTNO_NODE, smm_players[player].pos);
	printf("\nstart from %i(%s) (%i)\n", smm_players[player].pos, smmObj_getObjectName(ptr), step);
	for (i=0;i<step;i++)
	{
		smm_players[player].pos = (smm_players[player].pos + 1)%smm_board_nr;
		ptr = smmdb_getData(LISTNO_NODE, smm_players[player].pos);
		printf("  => moved to %i(%s)\n", smm_players[player].pos, smmObj_getObjectName(ptr));
		if (smm_players[player].pos == 0)
		{
			smm_players[player].energy += homeEnergy;
			printf("     (passed HOME: energy +%i => %i)\n", homeEnergy, smm_players[player].energy);
			
		}
	}
 
}

void printPlayerStatus(void)
{
	int i;
	void *ptr;
	for (i=0;i<smm_player_nr;i++)
	{
		ptr = smmdb_getData(LISTNO_NODE, smm_players[i].pos);
		printf( "%s - position : %i(%s), credit : %i, energy : %i\n", 
		smm_players[i].name, smm_players[i].pos, smmObj_getObjectName(ptr), smm_players[i].credit, smm_players[i].energy );
		
	}
}

void generatePlayers(int n, int initEnergy) //generate a new player
{
	int i;
	
	smm_players = (smm_player_t*)malloc(n*sizeof(smm_player_t));
	for (i=0;i<n;i++)
	{
		smm_players[i].pos = 0;
		smm_players[i].credit = 0;
		smm_players[i].energy = initEnergy;
		smm_players[i].flag_graduated = 0;
		smm_players[i].flag_experiment = 0;
		smm_players[i].exp_criterion = 0;
		
		printf("Input %i-th player name:",i+1);
		scanf("%s", &smm_players[i].name[0]);
		fflush(stdin);
	}
}


int rolldie(int player)
{
    char c;
    printf(" Press any key to roll a die (press g to see grade): ");
    c = getchar();
    //fflush(stdin);
    
#if 1
    if (c == 'g')
        printGrades(player);
#endif
    
    return (rand()%MAX_DIE + 1);
}


//action code when a player stays at a node
void actionNode(int player)
{
	void *ptr = smmdb_getData(LISTNO_NODE, smm_players[player].pos);
	int type = smmObj_getNodeType(ptr);
	int credit = smmObj_getNodeCredit(ptr);
    int energy = smmObj_getNodeEnergy(ptr);
    int grade;
    void *gradePtr;
	
    switch(type)
    {
    	case SMMNODE_TYPE_LECTURE:
    	if(findGrade(player, smmObj_getObjectName(ptr)) == NULL)
    	{
			smm_players[player].credit += credit;
    		smm_players[player].energy -= energy;
    		
    		grade = rand()%SMMNODE_MAX_GRADE;
    		
    		gradePtr = smmObj_genObject(smmObj_getObjectName(ptr), SMMNODE_OBJTYPE_GRADE, 
							type, credit, energy, grade);
			smmdb_addTail(LISTNO_OFFSET_GRADE+player, gradePtr);
			printf("took lecture %s (credit +%i, energy -%i) grade:%s\n",
				   smmObj_getObjectName(ptr), credit, energy, smmObj_getGradeName(grade));
		}
		else
			printf("already took lecture %s\n", smmObj_getObjectName(ptr));
    		break;
    		
		case SMMNODE_TYPE_RESTAURANT:
    		smm_players[player].energy += energy;
    		printf("restaurant: energy +%i => %i\n", energy, smm_players[player].energy);
			break;
			
		case SMMNODE_TYPE_LABORATORY:
			if (smm_players[player].flag_experiment == 1)
			{
				int die = rolldie(player);
				smm_players[player].energy -= energy;
				printf("experiment in LAB: die=%i criterion=%i (energy -%i => %i)\n",
						die, smm_players[player].exp_criterion, energy, smm_players[player].energy);
				if (die >= smm_players[player].exp_criterion)
				{
					printf("experiment success!\n");
					smm_players[player].flag_experiment = 0;
					smm_players[player].exp_criterion = 0;
				}
				else
				{
					printf("experiment fail. stay in LAB.\n");
				}
			}
			break;
			
		case SMMNODE_TYPE_HOME:
			printf("arrived HOME\n");
			break;
			
		case SMMNODE_TYPE_GOTOLAB:
			smm_players[player].flag_experiment = 1;
			smm_players[player].exp_criterion = (rand()%MAX_DIE) + 1;
			printf("gotoLab: criterion=%i, move to LAB(%i)\n", smm_players[player].exp_criterion, smm_lab_index);
			if (smm_lab_index >= 0)
				smm_players[player].pos = smm_lab_index;
			break;
			
		case SMMNODE_TYPE_FOODCHANCE:
			if (smm_food_nr > 0)
			{
				int idx = rand()%smm_food_nr;
				void *fptr = smmdb_getData(LISTNO_FOODCARD, idx);
				int gain = smmObj_getObjectEnergy(fptr);
				smm_players[player].energy += gain;
				printf("foodChance: %s (energy +%i => %i)\n", smmObj_getObjectName(fptr), gain, smm_players[player].energy);
				
			}
			break;
			
		case SMMNODE_TYPE_FESTIVAL:
			if (smm_festival_nr > 0)
			{
				int idx = rand()%smm_festival_nr;
				void *mptr = smmdb_getData(LISTNO_FESTCARD, idx);
				printf("festival mission: %s\n", smmObj_getObjectName(mptr));
			}
			break;
			
        default:
            break;
    }
}



int main(int argc, const char * argv[]) {
    
    FILE* fp;
    char name[MAX_CHARNAME];
    int type;
    int credit;
    int energy;
    int turn;
    
    smm_board_nr = 0;
    smm_food_nr = 0;
    smm_festival_nr = 0;
    
    srand(time(NULL));
    
    
    //1. import parameters ---------------------------------------------------------------------------------
    //1-1. boardConfig 
    if ((fp = fopen(BOARDFILEPATH,"r")) == NULL)
    {
        printf("[ERROR] failed to open %s. This file should be in the same directory of SMMarble.exe.\n", BOARDFILEPATH);
        getchar();
        return -1;
    }
    
    printf("Reading board component......\n");
    while ( fscanf(fp, "%s %i %i %i", name, &type, &credit, &energy) == 4 ) //read a node parameter set
    {
        //store the parameter set
        void* ptr;
        printf("%s %i %i %i\n", name, type, credit, energy);
        ptr = smmObj_genObject(name, SMMNODE_OBJTYPE_BOARD, type, credit, energy, 0);
        smmdb_addTail(LISTNO_NODE, ptr);
        smm_board_nr = smmdb_len(LISTNO_NODE);
    }
    fclose(fp);
    printf("Total number of board nodes : %i\n", smm_board_nr);
    
    
#if 1
    //2. food card config 
    if ((fp = fopen(FOODFILEPATH,"r")) == NULL)
    {
        printf("[ERROR] failed to open %s. This file should be in the same directory of SMMarble.exe.\n", FOODFILEPATH);
        return -1;
    }
    
    printf("\n\nReading food card component......\n");
    while (fscanf(fp, "%s %i", name, &energy) == 2) //read a food parameter set
    {
        //store the parameter set
        void *ptr;
        printf("%s %i\n", name, energy);
        ptr = smmObj_genObject(name, SMMNODE_OBJTYPE_FOOD, 0, 0, energy, 0);
        smmdb_addTail(LISTNO_FOODCARD, ptr);
    }
    fclose(fp);
    smm_food_nr = smmdb_len(LISTNO_FOODCARD);
    printf("Total number of food cards : %i\n", smm_food_nr);
    
    
    
    //3. festival card config 
    if ((fp = fopen(FESTFILEPATH,"r")) == NULL)
    {
        printf("[ERROR] failed to open %s. This file should be in the same directory of SMMarble.exe.\n", FESTFILEPATH);
        return -1;
    }
    
    printf("\n\nReading festival card component......\n");
    while (fscanf(fp, "%s", name) == 1) //read a festival card string
    {
        //store the parameter set
        void *ptr;
        printf("%s\n", name);
        ptr = smmObj_genObject(name, SMMNODE_OBJTYPE_FEST, 0, 0, 0, 0);
        smmdb_addTail(LISTNO_FESTCARD, ptr);
    }
    fclose(fp);
    smm_festival_nr = smmdb_len(LISTNO_FESTCARD);
    printf("Total number of festival cards : %i\n", smm_festival_nr);
    
#endif
    
    //2. Player configuration ---------------------------------------------------------------------------------
    
    do
    {
        //input player number to player_nr
        printf("Input player number:");
        scanf("%d", &smm_player_nr);
        fflush(stdin);
        
        if (smm_player_nr <= 0 || smm_player_nr > MAX_PLAYER)
        	printf("Invalid player number!\n");
    }
    while (smm_player_nr <= 0 || smm_player_nr > MAX_PLAYER);
    
    
    generatePlayers(smm_player_nr,smmObj_getObjectEnergy(smmdb_getData(LISTNO_NODE, 0)));
    smm_lab_index = findLaboratoryIndex();
    printf("LAB index : %i\n", smm_lab_index);
	
	turn = 0;

    //3. SM Marble game starts ---------------------------------------------------------------------------------
    while (isGraduated() == 0) //is anybody graduated?
    {
        int die_result;
        
        //4-1. initial printing
        printPlayerStatus();
        {
        	void *cptr = smmdb_getData(LISTNO_NODE, smm_players[turn].pos);
        	if (smm_players[turn].flag_experiment == 1 && smmObj_getNodeType(cptr) == SMMNODE_TYPE_LABORATORY)
        	{
        		actionNode(turn);
        		turn = (turn + 1)%smm_player_nr;
        		continue;
			}
		}
        
        //4-2. die rolling (if not in experiment)
        die_result = rolldie(turn);
        
        //4-3. go forward
        goForward(turn, die_result);
		//pos = pos + 2;

		
		//4-4. take action at the destination node of the board
        actionNode(turn);
        if (smm_players[turn].pos == 0 && smm_players[turn].credit >= GRADUATE_CREDIT)
        {
        	smm_players[turn].flag_graduated = 1;
        	printGrades(turn);
		}
        
        //4-5. next turn
        turn = (turn + 1)%smm_player_nr;
    }
    
    free(smm_players);

    return 0;
}
