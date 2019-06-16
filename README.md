<div style = "font-size: 2em;"><center >用lex和bison实现对数据库语言的解析</center></div>



# 题目

通过lex, bison，c语言实现对sql语句的制导功能。实现对数据的物理存储。测试样例如下：



# 实习目标

熟练用工具实现编程语言的词法分析过程，语法分析工程以及语义制导过程。



# 实验环境

操作系统：win10

编译环境：cygwin+gcc+lex+bison

IDE: CLion

具体的文件的依赖关系可以看makefile。

# 物理结构的设计

## isql.db

本文件主要存储的是数据库的数据的信息，相当于是数据的数据。

下面说明几个常变量的含义：

`PAGE_SIZE = 2048`: 表示的是一个页表的大小为2048Bytes, 用于基地址寻址用。

`第0页：`

![](https://ws1.sinaimg.cn/large/a7ded905ly1g3f3i7jn3bj20h10400so.jpg)

存放数据的一些基本的属性，可拓展性较大，总共占28bits。

可以看到table数量从18开始计数的，这里实际上就是给每个表一个编号，因为`0~17`号存着一些有用的数据，所以该编号就从18号开始。

`第1页：`

![](https://ws1.sinaimg.cn/large/a7ded905ly1g3f3ibymqij213n0370sn.jpg)

保存着数据库的基本的属性。一个数据组大小是64bits，那么一共可以存放2048/64=`32`个数据库信息项。



`第2~17页：`

![](https://ws1.sinaimg.cn/large/a7ded905ly1g3f3imiqv7j213q035aa3.jpg)

该数据项一个PAGE的大小是1024bits，所以每一个数据库能存放1024/64=16个表。

（17-2+1）*2048/1024 = 32,正好与我们之前算的能存32个数据库的数字相吻合。



`18~无穷$`：

![](https://ws1.sinaimg.cn/large/a7ded905ly1g3f3ihr9a6j213n036t8r.jpg)

存放的是表的属性的信息，一个表能存放2048/64 = 32个属性。



## isql.dat

`第0页：`

保存的是基本的数据。

`第1页：`

目前没有什么用，用于用户信息维护的拓展。

`2~65页：`

用户信息。

`65页之后:`

存放的是具体的数据，这些数据能存2048bits,也就是说，如果一个变量占4bits, 那么这个数据项就能保存2048/4=512项。注意：因为数据保存是线性的，所以在删除数据的时候不能以链表的形式进行删除。我用了一个特别蠢的方法，将要删除的特征保存下来，当扫描的时候满足条件的时候就不显示，目前是这个系统的一个小漏洞。



# 项目的结构

之前写的程序没有过多的涉及多个c文件之间的访问调用关系，导致最开始写这个编译器的时候，我所有的逻辑函数都是在`.y`文件里面实现的，后面越写越不想写，后来发现了更加科学的逻辑结构。

`name.c`:实现若干的函数逻辑关系，方便其他的`.c`文件进行调用。`#include "name.h" `必须用双引号而不能用尖括号，因为这类头文件是我们自己定义的，而不是标准库进行定义的。

`name.h`里面可以声明结构体，声明若干`name.c` 里面的逻辑函数。

然后就能进行愉快的分模块进行编程了。

bison进行变量的引用的时候，要使用`extern` 关键词进行相应的声明。

# Lex词法分析

我们主要需要解析的词性主要有下面的几类：

1. 数字
2. 字符串
3. sql的关键词，如：`SELECT`, `WHERE`等等
4. 运算符



## 进行正则匹配的规则



# Bison语法分析

## union

里面就是对所有的结构体类型进行重新的命名，并且为`yylval`结构体变量所调用。

## token

声明所有的关键词

## left

定义运算符的优先级，是左结合的还是右结合的。

## nonassoc

表示运算符是没有结合性的

## type

告诉后面的$num 是什么类型的变量。

逻辑替换的开始的符号：`stmt_list`

最后就要书写语法的分析了，设计还是相对比较简单的，大部分是线性的替换，除了`WHERE`语句里面的逻辑树的建立，这是逻辑判断的核心的数据结构。



# 语法制导翻译设计

## 判定树

一般的判定的过程都是线性的，`WHERE`的判断需要设计一棵判定树，下面稍微说明判定树的设计理念：

例子：`SNAME = 'TIAN' AND SAGE = 22`

![](E:\16030130096 田宝林\大三下\6-编译原理\5.png)



可以造出如上图所示的树形结构。

当我们需要判断某一条记录是否符合条件的时候，我们就去填充`SNAME`和`SAGE`里面的值，然后遍历该判定树，根据根节点返回的值来判断是否是符合要求的记录。

## 线性表

当我们有多个表需要查询的时候，我们以链表将其存成线性的结构，数据的查询。



# 实现与测试

主要使用一下的数据进行测试：

```sql
//测试CREATE DATABASE SHOW DATABASES DROP DATABASE USE DATABASE
CREATE DATABASE XJGL;
CREATE DATABASE JUST_FOR_TEST;
CREATE DATABASE JUST_FOR_TEST;
SHOW DATABASES;
DROP DATABASE JUST_FOR_TEST;
SHOW DATABASES;
USE XJGL;

//测试CREATE TABLE SHOW TABLES DROP TABLE
CREATE TABLE STUDENT(SNAME CHAR(20),SAGE INT,SSEX INT);CREATE TABLE COURSE(CNAME CHAR(20),CID INT);
CREATE TABLE CS(SNAME CHAR(20),CID INT);
CREATE TABLE TEST_TABLE(COL1 CHAR(22),COL2 INT,COL3 CHAR(22);
CREATE TABLE TEST_TABLE(COL1 CHAR(22),COL2 INT,COL3 CHAR(22);
SHOW TABLES;
DROP TABLE TEST_TABLE;
SHOW TABLES;

//测试INSERT INTO VALUES
INSERT INTO STUDENT(SNAME,SAGE,SSEX) VALUES ('ZHANGSAN',22,1);
INSERT INTO STUDENT VALUES ('LISI',23,0);
INSERT INTO STUDENT(SNAME,SAGE) VALUES ('WANGWU',21);
INSERT INTO STUDENT VALUES ('ZHAOLIU',22,1);
INSERT INTO STUDENT VALUES ('XIAOBAI',23,0);
INSERT INTO STUDENT VALUES ('XIAOHEI',19,0);
INSERT INTO COURSE(CNAME,CID) VALUES ('DB',1);
INSERT INTO COURSE (CNAME,CID) VALUES('COMPILER',2);
insert into COURSE (CNAME,CID) VALUES('C',3);

//测试单表查询
SELECT SNAME,SAGE,SSEX FROM STUDENT;
SELECT SNAME,SAGE FROM STUDENT;
SELECT * FROM STUDENT;
SELECT SNAME,SAGE FROM STUDENT WHERE SAGE=21;
SELECT SNAME,SAGE FROM STUDENT WHERE (((SAGE=21)));
SELECT SNAME,SAGE FROM STUDENT WHERE (SAGE>21) AND (SSEX=0);
SELECT SNAME,SAGE FROM STUDENT WHERE (SAGE>21) OR (SSEX=0);
SELECT * FROM STUDENT WHERE SSEX!=1;

// 测试多表查询
SELECT * FROM STUDENT;SELECT * FROM COURSE;
select * from student,course;
SELECT * FROM STUDENT,COURSE WHERE (SSEX=0) AND (CID=1);

//测试DELETE语句
SELECT * FROM STUDENT;
DELETE FROM STUDENT WHERE (SAGE>21) AND (SSEX=0);
SELECT * FROM STUDENT;

//测试UPDATE
SELECT * FROM STUDENT;
UPDATE STUDENT SET SAGE=21 WHERE SSEX=1;
SELECT * FROM STUDENT;
UPDATE STUDENT SET SAGE=27,SSEX=1 WHERE SNAME='ZHANGSAN';
SELECT * FROM STUDENT;


```



# 总结

该大作业约写了一个多星期，刚开始做的时候把语义制导的模块完全放在了.y文件里面，但是随着逻辑功能越来越复杂，在这个文件里面实现已经不可能了。于是学习了一下需要将逻辑实现函数封装在其他的文件里面，然后在.y文件里面调用。而且语义制导是十分关键并且耗费大量时间的模块，大约花了80%的时间在它的实现方面。

通过这次实习，极大的提升了自己的编码能力，为以后的软件工程开发打下了一定的基础。
