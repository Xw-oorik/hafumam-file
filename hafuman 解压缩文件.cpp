#include<iostream>
#include<assert.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<queue>
using namespace std;

#define DEBUG //注释掉了就走else，控制台就不会打印了
#ifdef DEBUG
//宏替换了 ...可变参替换成__VA_ARGS__ 后面%s 这些打印也宏替换
//如果不给可变参 ##就是用来把前面那个，号给吃掉，宏替换到__LINE__结束
#define LOG(frm , ...) fprintf(stderr,"[%s,%s,%d]"frm,__FILE__,__func__,__LINE__,##__VA_ARGS__)
#else
#define LOG(frm,...)
#endif

static const int BUFFLEN = 256;//写文件 读文件，缓冲区大小
static const int KIND = 256; // 字符种类个数
typedef int FREP[KIND];      // 频度数组


typedef unsigned char uchar;
typedef struct
{
	uchar ch;   // 字符类型
	int   frep; // 频度
}CharFrep;      //字符种类

typedef struct
{
	int total;  // 文件总的字节个数；
	int chkind; // 文件字符种类数
	CharFrep* frepdata;//指向字符种类的指针
}FileInfo;  //得到当前文件的信息

typedef struct
{
	uchar ch;
	int   cfreq; // 频度
	int   parent;
	int   leftchild;
	int   rightchild;
}hfmnode;
                     //哈夫曼树结构
typedef struct
{
	int chkind;  //文件字符种类数
	int sum;     //总共节点个数
	hfmnode* node;
}HuffmanTree;

struct Index_Fref//形成下标和权值的对应放进优先级队列里面
{                 //可以直接拿pair对组弄
	int index;
	int freq;
	operator int()const { return freq; }
};

typedef struct   
{
	uchar ch;
	char* code;
}hfmcode;               //哈夫曼结点 处理
typedef struct
{
	int chkind; //文件字符种类数
	hfmcode* coding;   //直接开辟256而不是按照chkind去开辟，为了提高效率
	                   //读到文件一个字符直接能映射到256里的下标，而不用一个个查找
}HuffManCode;

//获得源文件的信息
//创建配置文件  srcfile+freq.txt
//inputfilename 要压缩的源文件 
void GetFileInfo(const char* inputfilename, FileInfo* pfileinfo)
{
	assert(inputfilename != nullptr);
	assert(pfileinfo != nullptr);

	uchar buff[BUFFLEN] = { 0 };
	int kind = 0;
	int ret = 0;
	long sum = 0; // 文件总字节个数
	FREP frep = { 0 };//开辟了256字节大小
	FILE* srcfile = nullptr;
	errno_t tag = fopen_s(&srcfile, inputfilename, "rb");
	if (tag)
	{
		LOG(" 文件打开失败\n");
		exit(EXIT_FAILURE);
	}
	fseek(srcfile, 0, SEEK_END); //将文件位置指示符移动到文件中的指定位置
	sum = ftell(srcfile);        //返回当前的文件位置指示值
	rewind(srcfile);             //将文件位置指示器移动到文件首
	//统计每个字节出现的频率
	//用fgets只能读到255个字节，因为读的是char类型，后面会有/0
	while ((ret = fread(buff, sizeof(uchar), BUFFLEN, srcfile)) > 0)
	{
		for (int i = 0; i < ret; ++i)
		{

			frep[buff[i]] += 1;
			if (frep[buff[i]] == 1)
			{
				kind += 1;
			}
		}
	}
	fclose(srcfile);
	srcfile = nullptr;

	// 将存在的字符筛选出来
	//记录文件信息
	pfileinfo->total = sum;
	pfileinfo->chkind = kind;
	//pfileinfo->frepdata = (CharFrep*)malloc(sizeof(CharFrep) * kind);
	pfileinfo->frepdata = (CharFrep*)calloc(kind, sizeof(CharFrep));
	if (nullptr == pfileinfo->frepdata)
	{
		LOG("申请frepdata 空间失败\n");
		exit(EXIT_FAILURE);
	}

	int k = 0;
	for (int i = 0; i < KIND; ++i)
	{
		if (frep[i] != 0)//频度数组
		{
			pfileinfo->frepdata[k].ch = static_cast<uchar>(i);
			pfileinfo->frepdata[k].frep = frep[i];
			k += 1;
		}
	}
	// 创建配置文件  解压的时候要根据这个信息去解压
	FILE* freqfile = nullptr;
	const int n = 128;
	int len = strlen(inputfilename);
	char freqfilename[n] = { 0 };

	strcpy_s(freqfilename, n, inputfilename);
	strcpy_s(&freqfilename[len - 4], n - len, "frep.txt");
	tag = fopen_s(&freqfile, freqfilename, "wb");
	if (tag)
	{
		LOG(" 文件打开失败\n");
		exit(EXIT_FAILURE);
	}
	// 配置文件结构 文件总字符数;文件字符种类数; 字符值:频度
	fwrite(&pfileinfo->total, sizeof(int), 1, freqfile);
	fwrite(&pfileinfo->chkind, sizeof(int), 1, freqfile);
	for (int i = 0; i < kind; ++i)
	{
		//这块做一个测试打印，把此文件所有出现的字符和对应频度打出来
		printf("%d => %d\n", pfileinfo->frepdata[i].ch, pfileinfo->frepdata[i].frep);
		fwrite(&pfileinfo->frepdata[i], sizeof(CharFrep), 1, freqfile);
	}
	fclose(freqfile);
	freqfile = nullptr;
}
void CreateHuffManTree(FileInfo* pfileinfo, HuffmanTree* phfm)
{
	assert(pfileinfo != nullptr);
	assert(phfm != nullptr);
	std::priority_queue<Index_Fref, std::vector<Index_Fref>, std::greater<Index_Fref> > qu;
	phfm->chkind = pfileinfo->chkind;
	phfm->sum = pfileinfo->chkind * 2;
	phfm->node = (hfmnode*)calloc(phfm->sum, sizeof(hfmnode));
	if (nullptr == phfm->node)
	{
		LOG("申请hfmnode 空间失败!");
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < phfm->chkind; ++i)
	{
		phfm->node[i].ch = pfileinfo->frepdata[i].ch;
		phfm->node[i].cfreq = pfileinfo->frepdata[i].frep;
		qu.push(Index_Fref{ i,phfm->node[i].cfreq });
	}
	for (int i = 0; i < phfm->sum; ++i)
	{
		phfm->node[i].parent = -1;
	}
	int k = phfm->chkind;
	while (!qu.empty())
	{
		if (qu.empty()) break;
		Index_Fref left = qu.top(); qu.pop();
		if (qu.empty()) break;
		Index_Fref right = qu.top(); qu.pop();
		phfm->node[k].cfreq = left.freq + right.freq;
		phfm->node[k].leftchild = left.index;
		phfm->node[k].rightchild = right.index;
		phfm->node[left.index].parent = k;
		phfm->node[right.index].parent = k;
		qu.push(Index_Fref{ k,phfm->node[k].cfreq });
		k += 1;
	}
	LOG("huffmantree 建立完成！");
#ifdef  DEBUG		
	printf("chkind : %d \n", phfm->chkind);
	printf("sum: %d \n", phfm->sum);
	printf("\nIndex ch     Frep     Parent    LChild    RChild\n");
	for (int i = 0; i < phfm->sum; ++i)
	{
		printf("%5d %5c  %5d %5d %5d %5d\n", i,
			phfm->node[i].ch,
			phfm->node[i].cfreq,
			phfm->node[i].parent,
			phfm->node[i].leftchild,
			phfm->node[i].rightchild);
	}
#endif 

}
void CreateHuffManCode(FileInfo* pfileinfo, HuffManCode* phuffmancode)
{
	assert(pfileinfo != nullptr);
	assert(phuffmancode != nullptr);

	HuffmanTree hfm = { 0 };
	char* code = (char*)calloc(pfileinfo->chkind + 1, sizeof(char));
	if (nullptr == code)
	{
		LOG("申请 code 空间失败!");
		exit(EXIT_FAILURE);
	}
	CreateHuffManTree(pfileinfo, &hfm);
	phuffmancode->chkind = pfileinfo->chkind;
	phuffmancode->coding = (hfmcode*)calloc(KIND, sizeof(hfmcode));
	if (nullptr == phuffmancode->coding)
	{
		LOG("申请 phuffmancode 空间失败\n");
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < hfm.chkind; ++i)
	{
		int k = hfm.chkind;
		code[k] = '\0';
		int c = i;
		int pa = hfm.node[c].parent;
		while (pa != -1)
		{
			code[--k] = hfm.node[pa].leftchild == c ? '0' : '1';
			c = pa;
			pa = hfm.node[c].parent;
		}
		int len = hfm.chkind - k + 1;
		phuffmancode->coding[hfm.node[i].ch].ch = hfm.node[i].ch;
		phuffmancode->coding[hfm.node[i].ch].code = (char*)calloc(len + 1, sizeof(char));
		if (nullptr == phuffmancode->coding[hfm.node[i].ch].code)
		{
			LOG("申请编码空间失败\n");
			exit(EXIT_FAILURE);
		}
		strcpy_s(phuffmancode->coding[hfm.node[i].ch].code, len, &code[k]);
	}
	LOG("构建fuffmancode 成功");
#ifdef DEBUG
	for (int i = 0; i < KIND; ++i)
	{
		if (phuffmancode->coding[i].code != nullptr)
		{
			printf("%c : %d => %s \n", phuffmancode->coding[i].ch, phuffmancode->coding[i].ch, phuffmancode->coding[i].code);
		}
	}
#endif

	for (int i = 0; i < hfm.sum; ++i)
	{
		free(hfm.node);
		hfm.node = nullptr;
	}
}

//压缩
//CompressFile(&huffmancode, &fileinfo, inputfile,outputfile);
void CompressFile(HuffManCode* phfc, FileInfo* pfileinfo, const char* inputfile, const char* outputfile)
{
	assert(phfc != nullptr);
	assert(pfileinfo != nullptr);
	assert(inputfile != nullptr);
	assert(outputfile);
	FILE* srcfile = nullptr, * destfile = nullptr;
	errno_t tag = fopen_s(&srcfile, inputfile, "rb");//打开源文件
	if (tag)
	{
		LOG("open file %s error \n", inputfile);
		exit(EXIT_FAILURE);
	}
	tag = fopen_s(&destfile, outputfile, "wb"); //打开目标文件
	if (tag)
	{
		LOG("open file %s error \n", outputfile);
		exit(EXIT_FAILURE);
	}
	uchar buff[BUFFLEN] = { 0 };
	uchar writebuff[BUFFLEN] = { 0 };
	int wpos = 0;
	int ret = 0;
	uchar tmp = 0;
	uchar bitpos = 0x80;  //1000 0000
	int total = 0;
	rewind(srcfile);

	fwrite(&pfileinfo->total, sizeof(int), 1, destfile);
	while ((ret = fread(buff, sizeof(uchar), BUFFLEN, srcfile)) > 0)
	{
		for (int i = 0; i < ret; ++i)
		{
			if (phfc->coding[buff[i]].code == nullptr)
			{
				printf("error %d \n", buff[i]);
				exit(1);
			}
			for (int j = 0; phfc->coding[buff[i]].code[j] != '\0'; ++j)
			{
				tmp |= (phfc->coding[buff[i]].code[j] == '1') ? bitpos : 0;
				bitpos = bitpos >> 1;
				if (bitpos == 0) // 一个字节数据完成
				{
					bitpos = 0x80;
					writebuff[wpos++] = tmp;
					tmp = 0;
					if (wpos == BUFFLEN)
					{
						fwrite(writebuff, sizeof(uchar), wpos, destfile);
						total += wpos;
						wpos = 0;
					}
				}
			}
		}
	}
	if (bitpos != 0)
	{
		writebuff[wpos++] = tmp;
	}
	if (wpos > 0)
	{
		total += wpos;
		fwrite(writebuff, sizeof(uchar), wpos, destfile);
	}
	LOG("压缩文件成功\n");
	fclose(destfile);
	fclose(srcfile);
	destfile = nullptr;
	srcfile = nullptr;

	free(pfileinfo->frepdata);
	pfileinfo->frepdata = nullptr;
	for (int i = 0; i < phfc->chkind; ++i)
	{
		free(phfc->coding[i].code);
		phfc->coding[i].code = nullptr;
	}
	free(phfc->coding);
	phfc->coding = nullptr;
}
void HuffManCompressFile(const char* inputfile, const char* outputfile)
{
	assert(inputfile != nullptr);
	assert(outputfile != nullptr);
	FileInfo fileinfo = {};
	HuffManCode huffmancode = {};
	GetFileInfo(inputfile, &fileinfo);
	LOG("使用文件信息构建哈夫曼编码\n");
	CreateHuffManCode(&fileinfo, &huffmancode);
	LOG("使用哈夫曼编码压缩文件\n");
	CompressFile(&huffmancode, &fileinfo, inputfile, outputfile);
}
// 压缩文件
int Compress()
{
	const int LEN = 128;
	char inputfilename[LEN] = {};
	char outputfilename[LEN] = {};
	printf("请输入源文件名 \n");
	scanf_s("%s", inputfilename, LEN);
	printf("请输入压缩文件名\n");
	scanf_s("%s", outputfilename, LEN);
	HuffManCompressFile(inputfilename, outputfilename);
	return 0;
}

void ConfigToHuffmanTree(const char* config, HuffmanTree* phft)
{
	FileInfo fileinfo = { 0 };
	FILE* confile = nullptr;
	int ret = 0;
	errno_t tag = fopen_s(&confile, config, "rb");
	if (tag)
	{
		LOG("open file %s  fail \n", config);
		exit(EXIT_FAILURE);
	}
	if ((ret = fread(&fileinfo.total, sizeof(int), 1, confile)) <= 0)
	{
		LOG("read config file fail \n");
		exit(EXIT_FAILURE);
	}
	if ((ret = fread(&fileinfo.chkind, sizeof(int), 1, confile)) <= 0)
	{
		LOG("read config file fail \n");
		exit(EXIT_FAILURE);
	}
	fileinfo.frepdata = (CharFrep*)calloc(fileinfo.chkind, sizeof(CharFrep));
	if (nullptr == fileinfo.frepdata)
	{
		LOG("申请frepdata 空间失败\n");
		exit(EXIT_FAILURE);
	}
	if ((ret = fread(fileinfo.frepdata, sizeof(CharFrep), fileinfo.chkind, confile)) <= 0)
	{
		LOG("read config file fail \n");
		exit(EXIT_FAILURE);
	}
	fclose(confile);
	confile = nullptr;
	CreateHuffManTree(&fileinfo, phft);
	free(fileinfo.frepdata);
	fileinfo.frepdata = nullptr;
	LOG("哈夫曼树还原 \n");
}
//解压缩
void DeCompress(const char* newfilename, const char* compfilename, HuffmanTree* phft)
{
	uchar buff[BUFFLEN] = { 0 };
	uchar writebuff[BUFFLEN] = { 0 };
	int wpos = 0;
	int ret = 0;
	uchar tmp = 0;
	uchar bitpos = 0x80;  //1000 0000
	int total = 0;
	FILE* newfile = nullptr;
	FILE* compfile = nullptr;
	errno_t tag = fopen_s(&newfile, newfilename, "wb");
	if (tag)
	{
		LOG("open file %s fail \n", newfilename);
		exit(EXIT_FAILURE);
	}
	tag = fopen_s(&compfile, compfilename, "rb");
	if (tag)
	{
		LOG("open file %s fail \n", compfilename);
		exit(EXIT_FAILURE);
	}
	fread(&total, sizeof(int), 1, compfile);
	bitpos = 0x80;
	wpos = 0;
	int npos = phft->sum - 2;
	while ((ret = fread(buff, sizeof(uchar), BUFFLEN, compfile)) > 0)
	{
		int i = 0;
		while (i < ret)
		{
			do
			{
				if (phft->node[npos].leftchild == 0 && phft->node[npos].rightchild == 0)
				{
					break;
				}
				npos = (buff[i] & bitpos) ? phft->node[npos].rightchild : phft->node[npos].leftchild;
				bitpos = bitpos >> 1;
			} while (bitpos != 0);
			if (phft->node[npos].leftchild == 0 && phft->node[npos].rightchild == 0)
			{
				writebuff[wpos++] = phft->node[npos].ch;
				npos = phft->sum - 2;
				if (wpos == BUFFLEN)
				{
					fwrite(writebuff, sizeof(uchar), wpos, newfile);
					wpos = 0;
				}
				if (--total == 0) break;
			}
			if (bitpos == 0)
			{
				bitpos = 0x80;
				++i;
			}
		}
	}
	if (wpos > 0)
	{
		fwrite(writebuff, sizeof(uchar), wpos, newfile);
	}
	fclose(newfile);
	fclose(compfile);
	newfile = nullptr;
	compfile = nullptr;
}
void DeCompressFile(const char* newfile, const char* config, const char* compfile)
{
	assert(newfile != nullptr);
	assert(config != nullptr);
	assert(compfile != nullptr);
	HuffmanTree hft = { 0 };
	ConfigToHuffmanTree(config, &hft);
	DeCompress(newfile, compfile, &hft);
}
//解压文件
void Decompress()
{
	const int LEN = 128;
	char newfilename[LEN] = {};
	char configfilename[LEN] = {};
	char compfilename[LEN] = {};
	printf("请输入新文件名 \n");
	scanf_s("%s", newfilename, LEN);
	printf("请输入配置文件名\n");
	scanf_s("%s", configfilename, LEN);
	printf("请输入压缩文件名\n");
	scanf_s("%s", compfilename, LEN);
	DeCompressFile(newfilename, configfilename, compfilename);
}
int main()
{
#if 0
	//简单日志的打印测试
	int a = 10, b = 20;
	//   格式部分        数据部分
	LOG("a= %d b= %d\n", a, b);
	//宏替换了...可变参
	fprintf(stderr, "[%s,%s,%d]""a= %d b= %d\n", __FILE__, __func__, __LINE__, a, b);
	LOG("申请空间成功\n");
#else

	Compress();
	Decompress();
	return 0;
#endif
}