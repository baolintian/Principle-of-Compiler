/**************************************
 * filename isql_main.c
 * c file for SQL main func
 * cc -c -o isql_main.o isql_main.c
 * ************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "isql_main.h"

int DT_HANDLE;
int USER_NUM;
struct _fieldList FList[50];
int pFList;
//最多支持25次删除
struct _expr *drop_where_tree[25];
struct _fieldList drop_FList[25][50];
int drop_pFList[25];
char* drop_table_name[50];
int drop_table_number = 0;


struct ast_wordList* tempp = NULL;

char* read_D(FILE *fin,int offset,int length){
	char *tmp=(char*)malloc(sizeof(char)*length);
	fseek(fin,offset,SEEK_SET);
	fread(tmp,sizeof(char),length,fin);
	return tmp;
}

void write_D(FILE *fin,int offset,char *str){
	fseek(fin,offset,SEEK_SET);
	int num=fwrite(str,sizeof(char),strlen(str),fin);
	return ;
}

void fill_D(FILE *fin, int offset, int length, char t){
	int i;
	fseek(fin,offset,SEEK_SET);
	for(i=0;i<length;i++)
		fprintf(fin,"%c",t);
	return;
}

void emit(char *s, ...){
	extern yylineno;
	va_list ap;
	va_start(ap,s);

	printf("rpn: ");
	vfprintf(stdout,s,ap);
	printf("\n");
}

void yyerror (char *s, ...){
	extern yylineno;
	va_list ap;
	va_start(ap,s);
	fprintf(stderr,"%d:error: ",yylineno);
	vfprintf(stderr,s,ap);
	fprintf(stderr,"\n");
}

void eat_to_newline(){
	int c;
	while((c = getchar()) != EOF && c != '\n')
		;
}

int judge(FILE *fin,FILE *fout,char *tname,int power){ //还未加入使用
	int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
	int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,32)); //D中现有table数
	offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
	int i,u_page,u_num;
	for (i=0;i<table_num;i++){
		if (!strcmp(tname,read_D(fin,offset+i+T_HEAD_SIZE,T_NAME_SIZE))){
			u_page=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,T_PAGE))-18;
			u_num=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_USERNUM));
			break;
		}
	}
	offset=U_LISTPAGE*PAGE_SIZE+u_page*U_LISTSIZE;
	for (i=0;i<u_num;i++){
		if (USER_NUM==atoi(read_D(fout,offset,U_NUM))){
			printf("user_num:%d\n",USER_NUM);
			int org=atoi(read_D(fout,offset+U_NUM,U_POWER));
			printf("org:%d\n",org);
			if (org & power !=0) return 1;
			break;
		}
	}
	return 0;
}

struct ast_createDefinition *new_CDefinition(char *name, int data_type, int opt_length){
	struct ast_createDefinition *tmp = malloc(sizeof(struct ast_createDefinition));
	tmp->name=name;
	tmp->data_type=data_type;
	tmp->opt_length=opt_length;
	return tmp;
}

struct ast_createCol_list *add_createCol_list(struct ast_createCol_list *list,struct ast_createDefinition *nextDefinition){
	struct ast_createCol_list *tmp=list;
	while (tmp->next!=NULL) tmp=tmp->next;
	struct ast_createCol_list *added=malloc(sizeof(struct ast_createCol_list));
	added->createDefinition=nextDefinition;
	added->next=NULL;
	tmp->next=added;
	return list;
}

void create_Table(struct ast_createTable *table){
	FILE *fin,*fout;
	if (!(fin=fopen("isql.db","r+"))){
		printf("cannot open file isql.db\n");
	}
	if (!(fout=fopen("isql.dat","r+"))){
		printf("cannot open file isql.dat!\n");
	}

    char *tmp;
    int i;
    int flag = 0;
    int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
    int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,32));
    offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
    for (i=0;i<table_num;i++){
        tmp=read_D(fin,offset+i*T_HEAD_SIZE,T_NAME_SIZE);
        //printf("%s %s\n", tmp, table->name);
        if(tmp[0]!=0x00 && (!strcmp(tmp, table->name))){
            flag = 1;
            break;
        }
    }
    if(flag==1){
        printf("The table %s has been created\n", table->name);
        return ;
    }
    offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
    table_num=atoi(read_D(fin,offset+D_NAME_SIZE,32)); //D中现有table数
	fseek(fin,offset+D_NAME_SIZE,SEEK_SET);
	fprintf(fin,"%d",table_num+1);			//下一个table

	offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE+table_num*T_HEAD_SIZE;
	int tmpoff=offset;
	fill_D(fin,offset,T_HEAD_SIZE,null);    //初始化为空
	write_D(fin,offset,table->name);
	int num=atoi(read_D(fin,8,12));			//字段表地址页,给table一个全局的序号
	fseek(fin,offset+T_NAME_SIZE,SEEK_SET);
	fprintf(fin,"%d",num);
	fseek(fin,offset+T_NAME_SIZE+T_PAGE+T_USERNUM,SEEK_SET);
	fprintf(fin,"%d",0);							//初始表中没有记录
	fseek(fin,8,SEEK_SET);
	fprintf(fin,"%d",num+1);				//下一个字段表

	/*建立用户表*/
	//若处在其他用户的状态，那么会有两个用户有权限，一个是admin，还有一个是该用户
	if (USER_NUM==0) { fseek(fin,offset+T_NAME_SIZE+T_PAGE,SEEK_SET);fprintf(fin,"%d",1); }
	else {fseek(fin,offset+T_NAME_SIZE+T_PAGE,SEEK_SET);fprintf(fin,"%d",2); } 
	offset=U_LISTPAGE*PAGE_SIZE+(num-18)*U_LISTSIZE;
	fseek(fout,offset,SEEK_SET);			//admin用户
	fprintf(fout,"%d",0);
	fseek(fout,offset+U_NUM,SEEK_SET);
	fprintf(fout,"%d",15);
	if (USER_NUM!=0){
		offset=offset+U_WORDSIZE;
		fseek(fout,offset,SEEK_SET);
		fprintf(fout,"%d",USER_NUM);
		fseek(fout,offset+8,SEEK_SET);
		fprintf(fout,"%d",15);
	}

	struct ast_createCol_list *tmplist=table->col_list;
	offset = num*PAGE_SIZE;//num>=18
	int word=0;
	write_D(fin,offset+word*WORD_SIZE,tmplist->createDefinition->name);
	fseek(fin,offset+word*WORD_SIZE+W_NAME_SIZE,SEEK_SET);
	fprintf(fin,"%d",tmplist->createDefinition->data_type);
	fseek(fin,offset+word*WORD_SIZE+W_NAME_SIZE+W_TYPE,SEEK_SET);
	fprintf(fin,"%d",tmplist->createDefinition->opt_length);
	fseek(fin,offset+word*WORD_SIZE+W_NAME_SIZE+W_TYPE+W_LENGTH,SEEK_SET);
	num=atoi(read_D(fout,0,8));  //从16页开始
	fprintf(fin,"%d",num);
	while (tmplist->next!=NULL){
		tmplist=tmplist->next;
		word++;num++;
		write_D(fin,offset+word*WORD_SIZE,tmplist->createDefinition->name);
		fseek(fin,offset+word*WORD_SIZE+W_NAME_SIZE,SEEK_SET);
		fprintf(fin,"%d",tmplist->createDefinition->data_type);
		fseek(fin,offset+word*WORD_SIZE+W_NAME_SIZE+W_TYPE,SEEK_SET);
		fprintf(fin,"%d",tmplist->createDefinition->opt_length);
		fseek(fin,offset+word*WORD_SIZE+W_NAME_SIZE+W_TYPE+W_LENGTH,SEEK_SET);
	
		fprintf(fin,"%d",num);
	}
	fseek(fin,tmpoff+T_NAME_SIZE+T_PAGE+T_USERNUM+T_COUNT,SEEK_SET);
	fprintf(fin,"%d",word+1);			//字段数
	fseek(fout,0,SEEK_SET);
	fprintf(fout,"%d",num+1);
	//printf("test %d\n", num);
	fclose(fin);
	fclose(fout);
	return ;
}

struct ast_valList *add_Vals(struct ast_valList *list,struct _expr *nextExpr){
	struct ast_valList *tmp=list;
	while (tmp->next!=NULL) tmp=tmp->next;
	struct ast_valList *added=malloc(sizeof(struct ast_valList));
	added->expr=nextExpr;
	added->next=NULL;
	tmp->next=added;
	return list;
}

struct ast_wordList *add_wordList(struct ast_wordList *list, char *nextname){
	struct ast_wordList *tmp=list;
	while (tmp->next!=NULL) tmp=tmp->next;
	struct ast_wordList *added=malloc(sizeof(struct ast_wordList));
	added->name=nextname;
	added->next=NULL;
	tmp->next=added;
	return list;
}

void insert_Record(struct ast_insertRecord *record){
	struct ast_tableInto *tableinto=record->tableinto;
	struct ast_valList *vallist=record->vallist;

	FILE *fin,*fout;
	if (!(fin=fopen("isql.db","r+"))){
		printf("cannot open file isql.db\n");
	}
	if (!(fout=fopen("isql.dat","r+"))){
		printf("cannot open file isql.dat!\n");
	}

//	if (!judge(fin,fout,tableinto->name,P_INSERT)) goto end_insert;

	struct _table table;
	table.name=tableinto->name;
	table.D_HANDLE=DT_HANDLE;

	char *tmp;
	int i;
	int flag=0;
	int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
	//读取该数据库下表的个数
	int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,D_TABLE_NUM));
	offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
	for (i=0;i<table_num;i++){
		tmp=read_D(fin,offset+i*T_HEAD_SIZE,T_NAME_SIZE);
		//检测到某一个符合条件的表
		if (!strcmp(tmp,tableinto->name)){
			flag=1;
			fseek(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,SEEK_SET);
			table.T_HANDLE=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,T_PAGE));
			table.count=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_COUNT));
			fill_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_COUNT,null);
			fseek(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,SEEK_SET);
			fprintf(fin,"%d",table.count+1);
			table.words=atoi(read_D(fin,offset+(i+1)*T_HEAD_SIZE-T_WORDS,T_WORDS));
			break;
		}
	}
	if (!flag) { yyerror("Table:%s didn't exist!\n",tableinto->name); return ;}

	table.list=(struct _fieldList **)malloc(sizeof(struct _fieldList *)*table.words);
	offset=table.T_HANDLE*PAGE_SIZE;
	for (i=0;i<table.words;i++){
		table.list[i]=(struct _fieldList *)malloc(sizeof(struct _fieldList));
		table.list[i]->name=read_D(fin,offset+i*WORD_SIZE,W_NAME_SIZE);
		table.list[i]->type=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE,W_TYPE));
		table.list[i]->length=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE,W_LENGTH));
		table.list[i]->page=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE+W_LENGTH,W_PAGE));
	}

	struct ast_wordList *wordlist=tableinto->wordlist;
	//没有指定需要插入的数据
	if (wordlist==NULL){  //按顺序插入
		for (i=0;i<table.words;i++){
			fseek(fout,table.list[i]->page*PAGE_SIZE+table.count*table.list[i]->length,SEEK_SET);
			if (!table.list[i]->type)
				fprintf(fout,"%d",vallist->expr->intval);
			else fprintf(fout,"%s",vallist->expr->string);
			vallist=vallist->next;
		}
	}
	else{
		int **a=malloc(sizeof(int*)*table.words);
		for (i=0;i<table.words;i++){a[i]=malloc(sizeof(int));(*a[i])=0;}
		while (wordlist!=NULL){
			for (i=0;i<table.words;i++){
				if (!strcmp(table.list[i]->name,wordlist->name)){
					(*a[i])=1;
					fseek(fout,table.list[i]->page*PAGE_SIZE+table.count*table.list[i]->length,SEEK_SET);
					//printf("%s:",table.list[i]->name);
					if (!table.list[i]->type){
						fprintf(fout,"%d",vallist->expr->intval);
					//	printf("%d\n",vallist->expr->intval);
					}
					else {
						fprintf(fout,"%s",vallist->expr->string);
					//	printf("%s\n",vallist->expr->string);
					}
					break;
				}
			}
			wordlist=wordlist->next;
			vallist=vallist->next;
		}
		for (i=0;i<table.words;i++){
			if (!(*a[i])){
				fseek(fout,table.list[i]->page*PAGE_SIZE+table.count*table.list[i]->length,SEEK_SET);
				if(!table.list[i]->type){fprintf(fout,"%d",-1);}
				else fprintf(fout,"%s","NULL");
			}
		}
	}
	//这句话有什么作用？前面有一个goto的语句。
	end_insert:
	fclose(fin);
	fclose(fout);	
}

int judge_Where(struct _expr *tree, struct _fieldList FList1[]){

    if (tree==NULL || tree->type<3) return 1;
	int i1,i2;
	char *s1,*s2;
	int type;
	if (tree->type>2 && tree->type<9){
		switch (tree->left->type){ 
			case 0:if(FList1[pFList].type==0) {
					   type=0;i1=FList1[pFList].intval;
				   }
				   else { type=1; s1=FList1[pFList].charval; }
				   pFList++;
				   break;
			case 1:type=1; s1=tree->left->string; break;
			case 2:type=0; i1=tree->left->intval; break;
		}
		switch (tree->right->type){
			case 0:if(FList1[pFList].type==0) i2=FList1[pFList].intval;
					   else s2=FList1[pFList].charval;
				   pFList++;
				   break;
			case 1:s2=tree->right->string; break;
			case 2:i2=tree->right->intval; break;
		}
	}
	if (tree->type==9 || tree->type==10){
		i1=judge_Where(tree->left, FList1);
		i2=judge_Where(tree->right, FList1);
	}
	switch (tree->type){
		case 3:if (!type) tree->judge=(i1==i2?1:0);
				   else tree->judge=(strcmp(s1,s2)==0?1:0);
			   break;
		case 4:if (!type) tree->judge=(i1!=i2?1:0);
				   else tree->judge=(strcmp(s1,s2)!=0?1:0);
			   break;
		case 5:tree->judge=(i1<i2)?1:0;break;
		case 6:tree->judge=(i1>i2)?1:0;break;
		case 7:tree->judge=(i1<=i2)?1:0;break;
		case 8:tree->judge=(i1>=i2)?1:0;break;
		case 9:tree->judge=i1 & i2; break;
		case 10:tree->judge=i1 | i2; break;
	}
	return tree->judge;
}

void select_Record(struct ast_selectRecord *record){
	FILE *fin,*fout;
	if (!(fin=fopen("isql.db","r+"))){
		printf("cannot open file isql.db\n");
	}
	if (!(fout=fopen("isql.dat","r+"))){
		printf("cannot open file isql.dat!\n");
	}
//	if (!judge(fin,fout,record->name,P_SELECT)) goto end_select;

	struct _table table;
	table.name=record->name;
	table.D_HANDLE=DT_HANDLE;

	char *tmp;
	int i;
	int flag=0;
	int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
	int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,D_TABLE_NUM));
	offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
	for (i=0;i<table_num;i++){
		tmp=read_D(fin,offset+i*T_HEAD_SIZE,T_NAME_SIZE);
		if (!strcmp(tmp,table.name)){
			flag=1;
			fseek(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,SEEK_SET);
			table.T_HANDLE=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,T_PAGE));
			table.count=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_COUNT));
			table.words=atoi(read_D(fin,offset+(i+1)*T_HEAD_SIZE-T_WORDS,T_WORDS));
			break;
		}
	}
	if (!flag) { yyerror("Table:%s didn't exist!\n",table.name); return ;}

	table.list=(struct _fieldList **)malloc(sizeof(struct _fieldList *)*table.words);
	offset=table.T_HANDLE*PAGE_SIZE;
	//字段的个数table.words
	for (i=0;i<table.words;i++){
		table.list[i]=(struct _fieldList *)malloc(sizeof(struct _fieldList));
		table.list[i]->name=read_D(fin,offset+i*WORD_SIZE,W_NAME_SIZE);
		table.list[i]->type=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE,W_TYPE));
		table.list[i]->length=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE,W_LENGTH));
		table.list[i]->page=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE+W_LENGTH,W_PAGE));
	}

	struct ast_wordList *wordlist=record->wordlist;
	int j;
	//select * from table_name where opt_where;
	if (wordlist==NULL){
		for (i=0;i<table.words;i++) printf("%-16s",table.list[i]->name);
		printf("\n---------------------------------------------------\n");
		for (j=0;j<table.count;j++){
			flag=0;
			int flag2=0;
			int k;
			int test_delete = 0;
			int record_pList = pFList;
            if(drop_table_number>0){
                for(int a=0; a<drop_table_number; a++){
                    if(!strcmp(drop_table_name[a], table.name)){
                        for(k=0;k<drop_pFList[a];k++){
                            for (i=0;i<table.words;i++){
                                if(!strcmp(drop_FList[a][k].name,table.list[i]->name)){
                                    tmp=read_D(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,table.list[i]->length);
                                    drop_FList[a][k].type=table.list[i]->type;
                                    if (tmp[0]!=0x00){
                                        flag2=1;
                                        //将具体的属性印印射给FList中的数据变量。
                                        if (!drop_FList[a][k].type) drop_FList[a][k].intval=atoi(tmp);
                                        else drop_FList[a][k].charval=tmp;
                                    }
                                    break;
                                }
                            }
                        }
                        //此刻所有的要比较的属性都填好了。
                        //中间要借用这个游标，所以之前会有临时的记录
                        pFList = 0;
                        if(flag2 == 1 && judge_Where(drop_where_tree[a], drop_FList[a])){
                            test_delete = 1;
                            break;
                        }
                    }
                    if(test_delete) break;
                }
            }
            if(test_delete == 1) continue;
            flag2 = 0;
            pFList = record_pList;
			for(k=0;k<pFList;k++){
				for (i=0;i<table.words;i++){
					if(!strcmp(FList[k].name,table.list[i]->name)){

						tmp=read_D(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,table.list[i]->length);  //null没有处理
						FList[k].type=table.list[i]->type;
						if (tmp[0]!=0x00) {
							flag2=1;
							//将具体的属性印印射给FList中的数据变量。
							if (!FList[k].type) FList[k].intval=atoi(tmp);
							else FList[k].charval=tmp;
						}
						break;
					}
				}
			}
			if (pFList==0) flag2=1;
			pFList=0;
			if (flag2 && judge_Where(record->where_tree, FList)){
				for (i=0;i<table.words;i++){
					tmp=read_D(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,table.list[i]->length);
					if (tmp[0]!=0x00){
						printf("%-16s",tmp);
						flag=1;
					}
				}
				if (flag) printf("\n");
			}
		}
	}
	//select sel_feild from table_name where opt_where;
	else{
		struct ast_wordList *tmplist=wordlist;
		while (tmplist!=NULL) { printf("%-16s",tmplist->name);tmplist=tmplist->next; }
		printf("\n---------------------------------------------------\n");
		for (j=0;j<table.count;j++){
			tmplist=wordlist;
			flag=0;
			int k, i;
			int flag2 = 0;
            //插入判断树
            for(k=0;k<pFList;k++){
                for (i=0;i<table.words;i++){
                    if(!strcmp(FList[k].name,table.list[i]->name)){
                        tmp=read_D(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,table.list[i]->length);  //null没有处理
                        FList[k].type=table.list[i]->type;
                        if (tmp[0]!=0x00) {
                            flag2=1;
                            //将具体的属性印印射给FList中的数据变量。
                            if (!FList[k].type) FList[k].intval=atoi(tmp);
                            else FList[k].charval=tmp;
                        }
                        break;
                    }
                }
            }
            if (pFList==0) flag2=1;
            pFList=0;
            int can_print = 0;
            if (flag2 && judge_Where(record->where_tree, FList)){
                can_print = 1;
            }
            if(can_print == 0) continue;

			while (tmplist!=NULL){
				for (i=0;i<table.words;i++){
					if (!strcmp(tmplist->name,table.list[i]->name)){
						tmp=read_D(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,table.list[i]->length);
						if (tmp[0]!=0x00){
							printf("%-16s",tmp);
							flag=1;
						}
						break;
					}
				}
				tmplist=tmplist->next;
			}
			if (flag) printf("\n");
		}
	}
	end_select:
	fclose(fin);
	fclose(fout);
}
//记录选择的多个表的名字
struct ast_wordList* temp_attri[20];
int attri_depth;
struct Dicer{
    int num[10];
    char * attr[10];
    int col_number;
    char * name[10];
    int type[10];

}dicer;

void print_first_row(char* name){
    FILE *fin,*fout;
    if (!(fin=fopen("isql.db","r+"))){
        printf("cannot open file isql.db\n");
    }
    if (!(fout=fopen("isql.dat","r+"))){
        printf("cannot open file isql.dat!\n");
    }
    struct _table table;
    table.name = name;
    table.D_HANDLE=DT_HANDLE;

    char *tmp;
    int i;
    int flag=0;
    int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
    int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,D_TABLE_NUM));
    offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
    for (i=0;i<table_num;i++){
        tmp=read_D(fin,offset+i*T_HEAD_SIZE,T_NAME_SIZE);
        if (!strcmp(tmp,table.name)){
            flag=1;
            fseek(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,SEEK_SET);
            table.T_HANDLE=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,T_PAGE));
            table.count=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_COUNT));
            table.words=atoi(read_D(fin,offset+(i+1)*T_HEAD_SIZE-T_WORDS,T_WORDS));
            break;
        }
    }
    if (!flag) { yyerror("Table:%s didn't exist!\n",table.name); return ;}

    table.list=(struct _fieldList **)malloc(sizeof(struct _fieldList *)*table.words);
    offset=table.T_HANDLE*PAGE_SIZE;
    //字段的个数table.words
    for (i=0;i<table.words;i++){
        table.list[i]=(struct _fieldList *)malloc(sizeof(struct _fieldList));
        table.list[i]->name=read_D(fin,offset+i*WORD_SIZE,W_NAME_SIZE);
        table.list[i]->type=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE,W_TYPE));
        table.list[i]->length=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE,W_LENGTH));
        table.list[i]->page=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE+W_LENGTH,W_PAGE));
    }
    for (i=0;i<table.words;i++) {
        printf("%-16s",table.list[i]->name);
        dicer.name[dicer.col_number] = table.list[i]->name;
        dicer.type[dicer.col_number++] = table.list[i]->type;
    }
}

int build_judge_val(){
    char * tmp;
    int flag2 = 0;
    for(int k=0;k<pFList;k++){

        for (int i=0;i<dicer.col_number;i++){
            if(!strcmp(FList[k].name,dicer.name[i])){
                tmp=dicer.attr[i];
                FList[k].type=dicer.type[i];
                if (tmp[0]!=0x00) {
                    flag2=1;
                    //将具体的属性印印射给FList中的数据变量。
                    if (!FList[k].type) FList[k].intval=atoi(tmp);
                    else FList[k].charval=tmp;
                }
                break;
            }
        }
        if(flag2) {
           flag2=0;
        }
        else return 0;
    }

    pFList = 0;
    return 1;
}

void print_multi_attri(int depth, struct ast_selectRecord *selectRecord){
    FILE *fin,*fout;
    if (!(fin=fopen("isql.db","r+"))){
        printf("cannot open file isql.db\n");
    }
    if (!(fout=fopen("isql.dat","r+"))){
        printf("cannot open file isql.dat!\n");
    }
    struct _table table;
    table.name=temp_attri[depth]->name;
    table.D_HANDLE=DT_HANDLE;

    char *tmp;
    int i;
    int flag=0;
    int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
    int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,D_TABLE_NUM));
    offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
    for (i=0;i<table_num;i++){
        tmp=read_D(fin,offset+i*T_HEAD_SIZE,T_NAME_SIZE);
        if (!strcmp(tmp,table.name)){
            flag=1;
            fseek(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,SEEK_SET);
            table.T_HANDLE=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,T_PAGE));
            table.count=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_COUNT));
            table.words=atoi(read_D(fin,offset+(i+1)*T_HEAD_SIZE-T_WORDS,T_WORDS));
            break;
        }
    }
    if (!flag) { yyerror("Table:%s didn't exist!\n",table.name); return ;}

    table.list=(struct _fieldList **)malloc(sizeof(struct _fieldList *)*table.words);
    offset=table.T_HANDLE*PAGE_SIZE;
    //字段的个数table.words
    for (i=0;i<table.words;i++){
        table.list[i]=(struct _fieldList *)malloc(sizeof(struct _fieldList));
        table.list[i]->name=read_D(fin,offset+i*WORD_SIZE,W_NAME_SIZE);
        table.list[i]->type=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE,W_TYPE));
        table.list[i]->length=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE,W_LENGTH));
        table.list[i]->page=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE+W_LENGTH,W_PAGE));
    }
    int attri_number = 0;
    int temp = 0;
    for(int i=0; i<depth; i++){
        attri_number += dicer.num[i];
    }
    temp = attri_number;
    dicer.num[depth] = table.words;

    for (int j=0;j<table.count;j++) {
        attri_number = temp;
        int flag1 = 0;
        for (int i = 0; i < table.words; i++) {
            tmp = read_D(fout, table.list[i]->page * PAGE_SIZE + j * table.list[i]->length, table.list[i]->length);
            if (tmp[0] != 0x00) {
                //memset(dicer.attr[attri_number], 0x00, sizeof(dicer.attr[attri_number]));
                dicer.attr[attri_number++] = tmp;
                //printf("%-16s", tmp);
                flag1 = 1;
            }
        }
        if(!flag1)continue;
        if(depth == attri_depth-1){
            int flag = 0;
            //printf("test--------------%d\n", attri_number);
            for(int i=0; i<attri_number; i++){
                if(dicer.attr[i] == NULL) {
                    flag = 1;break;
                }
            }
            if(flag) continue;
            int judge_val = build_judge_val();

            if(!(judge_val&&judge_Where(selectRecord, FList))) continue;
            for(int i=0; i<attri_number; i++){
                printf("%-16s", dicer.attr[i]);
            }
            printf("\n");
        }
        else{
            print_multi_attri(depth+1, selectRecord);
        }
    }

}


void select_multi_table(struct ast_selectRecord *record, struct ast_wordList * table){
    FILE *fin,*fout;

    if (!(fin=fopen("isql.db","r+"))){
        printf("cannot open file isql.db\n");
    }
    if (!(fout=fopen("isql.dat","r+"))){
        printf("cannot open file isql.dat!\n");
    }


    int cnt = 0;
    while(table!=NULL){
        temp_attri[cnt] = table;
        table = table->next;
        cnt++;
    }
    {
        int offset = D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
        int table_number = read_D(fin, offset+D_NAME_SIZE, D_TABLE_NUM);
        memset(dicer.attr, 0, sizeof(dicer.attr));
        memset(dicer.num, 0, sizeof(dicer.attr));
        dicer.col_number = 0;
        memset(dicer.name, 0, sizeof(dicer.name));
        memset(dicer.type, 0, sizeof(dicer.type));
        for(int i=0; i<cnt; i++){
            char* name = temp_attri[i]->name;
            print_first_row(name);
        }
        printf("\n");
        attri_depth = cnt;

        print_multi_attri(0, record->where_tree);
    }
}

int find_UNUM(char *name){
	FILE *fin,*fout;
	if (!(fin=fopen("isql.db","r+"))){
		printf("cannot open file isql.db\n");
	}
	if (!(fout=fopen("isql.dat","r+"))){
		printf("cannot open file isql.dat!\n");
	}
	int num=atoi(read_D(fin,20,8));
	int offset=U_PAGE*PAGE_SIZE;
	int i;
	for (i=0;i<num;i++)
		if (!strcmp(name,read_D(fout,offset,24))) {
			fclose(fin);fclose(fout);
			return atoi(read_D(fout,offset+U_NAME,U_NUM));
		}
		else offset=offset+U_HEAD_SIZE;
}

void user_Manage(struct ast_permission *pchange){
	if (DT_HANDLE==-1) {yyerror("Please choose a DATABASE !"); return ;}
	struct ast_wordList *tablelist=pchange->tablelist;
	struct ast_wordList *userlist =pchange->userlist;
	FILE *fin,*fout;
	if (!(fin=fopen("isql.db","r+"))){
		printf("cannot open file isql.db\n");
	}
	if (!(fout=fopen("isql.dat","r+"))){
		printf("cannot open file isql.dat!\n");
	}

	char *tmp;
	int i;
	int flag;
	int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
	int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,D_TABLE_NUM));
	int users,usernum,k;
	struct ast_wordList *tmplist;

	while (tablelist!=NULL){
		flag=0;
		offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
		for (i=0;i<table_num;i++){
			tmp=read_D(fin,offset+i*T_HEAD_SIZE,T_NAME_SIZE);
			if (!strcmp(tmp,tablelist->name)){
				flag=1;
				users=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,T_PAGE))-18;
				usernum=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_USERNUM));
				break;
			}
		}
		if (!flag) { yyerror("Table:%s didn't exist!\n",tablelist->name); return ;}
		offset=U_LISTPAGE*PAGE_SIZE+users*U_LISTSIZE+usernum*U_WORDSIZE;
		k=0;
		tmplist=userlist;
		while (tmplist!=NULL){
			fseek(fout,offset,SEEK_SET);
			fprintf(fout,"%d",find_UNUM(tmplist->name));
			int org=atoi(read_D(fout,offset+U_NUM,U_POWER));
			if (!pchange->flag)
				pchange->permission=pchange->permission | org;
			else 
				pchange->permission=~pchange->permission & org;
			fseek(fout,offset+U_NUM,SEEK_SET);
			fprintf(fout,"%d",pchange->permission);
			offset=offset+U_WORDSIZE;
			tmplist=tmplist->next;
		}
		tablelist=tablelist->next;
	}
	fclose(fin);
	fclose(fout);
}

struct ast_UpdateCol *create_update_col(char *name, struct _expr *b){
    struct ast_UpdateCol *temp = malloc(sizeof(struct ast_UpdateCol));
    temp->name = name;
    if(b->type == 1) temp->charval = b->string, temp->type = 1;
    else temp->intval = b->intval, temp->type = 2;
    temp->next = NULL;
    //printf("haha %s %d\n", name, b->intval);
    return temp;
}

struct ast_UpdateCol *merge_update_col(struct ast_UpdateCol *a, struct ast_UpdateCol *b){
    struct ast_UpdateCol *tmp=a;
    while (tmp->next!=NULL) tmp=tmp->next;
    tmp->next=b;
    return a;
}

void traverse(struct ast_UpdateCol *head){
    while(head!=NULL){
        printf("now  %s\n", head->name);
        head = head->next;
    }
}

void process_update(struct ast_wordList* reference_table, struct ast_UpdateCol * col, struct _expr *where_tree){
    FILE *fin,*fout;
    if (!(fin=fopen("isql.db","r+"))){
        printf("cannot open file isql.db\n");
    }
    if (!(fout=fopen("isql.dat","r+"))){
        printf("cannot open file isql.dat!\n");
    }
//	if (!judge(fin,fout,record->name,P_SELECT)) goto end_select;

    struct _table table;
    table.name=reference_table->name;
    table.D_HANDLE=DT_HANDLE;

    char *tmp;
    int i;
    int flag=0;
    int offset=D_PAGE*PAGE_SIZE+DT_HANDLE*D_HEAD_SIZE;
    int table_num=atoi(read_D(fin,offset+D_NAME_SIZE,D_TABLE_NUM));
    offset=T_LIST_PAGE*PAGE_SIZE+DT_HANDLE*T_LIST_SIZE;
    for (i=0;i<table_num;i++){
        tmp=read_D(fin,offset+i*T_HEAD_SIZE,T_NAME_SIZE);
        if (!strcmp(tmp,table.name)){
            flag=1;
            fseek(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,SEEK_SET);
            table.T_HANDLE=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE,T_PAGE));
            table.count=atoi(read_D(fin,offset+i*T_HEAD_SIZE+T_NAME_SIZE+T_PAGE,T_COUNT));
            table.words=atoi(read_D(fin,offset+(i+1)*T_HEAD_SIZE-T_WORDS,T_WORDS));
            break;
        }
    }
    if (!flag) { yyerror("Table:%s didn't exist!\n",table.name); return ;}

    table.list=(struct _fieldList **)malloc(sizeof(struct _fieldList *)*table.words);
    offset=table.T_HANDLE*PAGE_SIZE;
    //字段的个数table.words
    for (i=0;i<table.words;i++){
        table.list[i]=(struct _fieldList *)malloc(sizeof(struct _fieldList));
        table.list[i]->name=read_D(fin,offset+i*WORD_SIZE,W_NAME_SIZE);
        table.list[i]->type=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE,W_TYPE));
        table.list[i]->length=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE,W_LENGTH));
        table.list[i]->page=atoi(read_D(fin,offset+i*WORD_SIZE+W_NAME_SIZE+W_TYPE+W_LENGTH,W_PAGE));
    }


    int j;
    //select * from table_name where opt_where;
        struct ast_UpdateCol *temp;
        for (j=0;j<table.count;j++){
            flag=0;
            temp = col;
            int flag2=0;
            int k;
            int test_delete = 0;
            int record_pList = pFList;
            if(drop_table_number>0){
                for(int a=0; a<drop_table_number; a++){
                    if(!strcmp(drop_table_name[a], table.name)){
                        for(k=0;k<drop_pFList[a];k++){
                            for (i=0;i<table.words;i++){
                                if(!strcmp(drop_FList[a][k].name,table.list[i]->name)){
                                    tmp=read_D(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,table.list[i]->length);
                                    drop_FList[a][k].type=table.list[i]->type;
                                    if (tmp[0]!=0x00){
                                        flag2=1;
                                        //将具体的属性印印射给FList中的数据变量。
                                        if (!drop_FList[a][k].type) drop_FList[a][k].intval=atoi(tmp);
                                        else drop_FList[a][k].charval=tmp;
                                    }
                                    break;
                                }
                            }
                        }
                        //此刻所有的要比较的属性都填好了。
                        //中间要借用这个游标，所以之前会有临时的记录
                        pFList = 0;
                        if(flag2 == 1 && judge_Where(drop_where_tree[a], drop_FList[a])){
                            test_delete = 1;
                            break;
                        }
                    }
                    if(test_delete) break;
                }
            }
            if(test_delete == 1) continue;
            flag2 = 0;

            for(k=0;k<pFList;k++){
                for (i=0;i<table.words;i++){
                    if(strcmp(FList[k].name,table.list[i]->name) == 0){
                        tmp=read_D(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,table.list[i]->length);  //null没有处理
                        FList[k].type=table.list[i]->type;
                        //printf("attri test %s\n", tmp);
                        if (tmp[0]!=0x00||strlen(tmp)) {
                            flag2=1;
                            //将具体的属性印印射给FList中的数据变量。
                            if (!FList[k].type) FList[k].intval=atoi(tmp);
                            else FList[k].charval=tmp;
                        }
                        break;
                    }
                }
            }
            if(!flag2) continue;
            if (pFList==0) flag2=1;
            pFList=0;

            if (flag2 && judge_Where(where_tree, FList)){
                while(temp!=NULL) {
                    int flag3 = 0;
                    char * name = temp->name;
                    //printf("test here %s %d", temp->name, temp->intval);
                    for (i = 0; i < table.words; i++) {
                        //这里应该去找列名
                        tmp = read_D(fin,offset+i*WORD_SIZE,W_NAME_SIZE);
                        fseek(fout,table.list[i]->page*PAGE_SIZE+j*table.list[i]->length,SEEK_SET);
                        if (!strcmp(name, tmp)) {
                            flag3 = 1;

                            if(temp->type == 1) {fprintf(fout,"%s",temp->charval);}
                            else {fprintf(fout,"%d",temp->intval);}
                        }
                    }
                    if(flag3 == 0){
                        printf("Update failed!\n");
                    }
                    temp = temp->next;
                }

            }
            pFList = record_pList;
        }
}
void init_attribution(void){
	//db :0页：8位当前数据库数+12位当前字段表地址+8位用户数
	//dat:0页：16位字段数据地址
	FILE *fin,*fout;
	if (!(fin=fopen("isql.db","r+"))){
		printf("cannot open file isql.db\n");
	}
	if (!(fout=fopen("isql.dat","r+"))){
		printf("cannot open file isql.dat!\n");
	}
	
	int num;
	num=atoi(read_D(fin,8,12));
	if (num==0) { fseek(fin,8,SEEK_SET); fprintf(fin,"%d",18); }
	num=atoi(read_D(fin,20,8));
	if (num==0) {
		fseek(fin,20,SEEK_SET); 
		fprintf(fin,"%d",1);

	}
	num=atoi(read_D(fout,0,16));
	if (num==0) { fseek(fout,0,SEEK_SET); fprintf(fout,"%d",66); }

	fclose(fin);
	fclose(fout);	

	DT_HANDLE=-1;
	USER_NUM=-1;
	pFList=0;
	return ;
}

void print_head(void){
	printf("\n*************** welcome to SQL ****************\n");
	printf("* Hello babydragon                               *\n");
	printf("************************************************\n\n");
	return ;
}

int main (){
	init_attribution();
	print_head();
	printf("> ");
	return yyparse();
}
