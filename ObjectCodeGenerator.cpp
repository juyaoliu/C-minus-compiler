#include "ObjectCodeGenarator.h"

bool isVar(string name) {
	return isalpha(name[0]);
}

bool isNum(string name) {
	return isdigit(name[0]);
}

bool isControlOp(string op) {
	if (op[0] == 'j' || op == "call" || op == "return" || op == "get") {
		return true;
	}
	return false;
}

VarInfomation::VarInfomation(int next, bool active) {
	this->next = next;
	this->active = active;
}

VarInfomation::VarInfomation(const VarInfomation&other) {
	this->active = other.active;
	this->next = other.next;
}

VarInfomation::VarInfomation() {

}

void VarInfomation::output(ostream& out) {
	out << "(";
	if (next == -1)
		out << "^";
	else
		out << next;
	out << ",";
	if (active)
		out << "y";
	else
		out << "^";

	out << ")";
}

QuaternaryWithInfo::QuaternaryWithInfo(Quaternary q, VarInfomation info1, VarInfomation info2, VarInfomation info3) {
	this->q = q;
	this->info1 = info1;
	this->info2 = info2;
	this->info3 = info3;
}

void QuaternaryWithInfo::output(ostream& out) {
	out << "(" << q.op << "," << q.src1 << "," << q.src2 << "," << q.des << ")";
	info1.output(out);
	info2.output(out);
	info3.output(out);
}

ObjectCodeGenerator::ObjectCodeGenerator() {

}

void ObjectCodeGenerator::storeVar(string reg, string var) {
	if (varOffset.find(var) != varOffset.end()) {//如果已经为*iter分配好了存储空间
		objectCodes.push_back(string("sw ") + reg + " " + to_string(varOffset[var]) + "($sp)");
	}
	else {
		varOffset[var] = top;
		top += 4;
		objectCodes.push_back(string("sw ") + reg + " " + to_string(varOffset[var]) + "($sp)");
	}
	Avalue[var].insert(var);
}

void ObjectCodeGenerator::releaseVar(string var) {
	for (set<string>::iterator iter = Avalue[var].begin(); iter != Avalue[var].end(); iter++) {
		if ((*iter)[0] == '$') {
			Rvalue[*iter].erase(var);
			if (Rvalue[*iter].size() == 0 && (*iter)[1] == 's') {
				freeReg.push_back(*iter);
			}
		}
	}
	Avalue[var].clear();
}

//为引用变量分配寄存器
string ObjectCodeGenerator::allocateReg() {
	//如果有尚未分配的寄存器，则从中选取一个Ri为所需要的寄存器R
	string ret;
	if (freeReg.size()) {
		ret = freeReg.back();
		freeReg.pop_back();
		return ret;
	}

	/*
	从已分配的寄存器中选取一个Ri为所需要的寄存器R。最好使得Ri满足以下条件：
	占用Ri的变量的值也同时存放在该变量的贮存单元中
	或者在基本块中要在最远的将来才会引用到或不会引用到。
	*/

	const int inf = 1000000;
	int maxNextPos = 0;
	for (map<string, set<string> >::iterator iter = Rvalue.begin(); iter != Rvalue.end(); iter++) {//遍历所有的寄存器
		int nextpos = inf;
		for (set<string>::iterator viter = iter->second.begin(); viter != iter->second.end(); viter++) {//遍历寄存器中储存的变量
			bool inFlag = false;//变量已在其他地方存储的标志
			for (set<string>::iterator aiter = Avalue[*viter].begin(); aiter != Avalue[*viter].end(); aiter++) {//遍历变量的存储位置
				if (*aiter != iter->first) {//如果变量存储在其他地方
					inFlag = true;
					break;
				}
			}
			if (!inFlag) {//如果变量仅存储在寄存器中，就看未来在何处会引用该变量
				for (vector<QuaternaryWithInfo>::iterator cIter = nowQuatenary; cIter != nowIBlock->codes.end(); cIter++) {
					if (*viter == cIter->q.src1 || *viter == cIter->q.src2) {
						nextpos = cIter - nowQuatenary;
					}
					else if (*viter == cIter->q.des) {
						break;
					}
				}
			}
		}
		if (nextpos == inf) {
			ret = iter->first;
			break;
		}
		else if (nextpos > maxNextPos) {
			maxNextPos = nextpos;
			ret = iter->first;
		}
	}

	for (set<string>::iterator iter = Rvalue[ret].begin(); iter != Rvalue[ret].end(); iter++) {
		//对ret的寄存器中保存的变量*iter，他们都将不再存储在ret中
		Avalue[*iter].erase(ret);
		//如果V的地址描述数组AVALUE[V]说V还保存在R之外的其他地方，则不需要生成存数指令
		if (Avalue[*iter].size() > 0) {
			//pass
		}
		//如果V不会在此之后被使用，则不需要生成存数指令
		else {
			bool storeFlag = true;
			vector<QuaternaryWithInfo>::iterator cIter;
			for (cIter = nowQuatenary; cIter != nowIBlock->codes.end(); cIter++) {
				if (cIter->q.src1 == *iter || cIter->q.src2 == *iter) {//如果V在本基本块中被引用
					storeFlag = true;
					break;
				}
				if (cIter->q.des == *iter) {//如果V在本基本块中被赋值
					storeFlag = false;
					break;
				}
			}
			if (cIter == nowIBlock->codes.end()) {//如果V在本基本块中未被引用，且也没有被赋值
				int index = nowIBlock - funcIBlocks[nowFunc].begin();
				if (funcOUTL[nowFunc][index].count(*iter) == 1) {//如果此变量是出口之后的活跃变量
					storeFlag = true;
				}
				else {
					storeFlag = false;
				}
			}
			if (storeFlag) {//生成存数指令
				storeVar(ret, *iter);
			}
		}
	}
	Rvalue[ret].clear();//清空ret寄存器中保存的变量

	return ret;
}

//为引用变量分配寄存器
string ObjectCodeGenerator::allocateReg(string var) {
	if (isNum(var)) {
		string ret = allocateReg();
		objectCodes.push_back(string("addi ") + ret + " $zero " + var);
		return ret;
	}

	for (set<string>::iterator iter = Avalue[var].begin(); iter != Avalue[var].end(); iter++) {
		if ((*iter)[0] == '$') {//如果变量已经保存在某个寄存器中
			return *iter;//直接返回该寄存器
		}
	}

	//如果该变量没有在某个寄存器中
	string ret = allocateReg();
	objectCodes.push_back(string("lw ") + ret + " " + to_string(varOffset[var]) + "($sp)");
	Avalue[var].insert(ret);
	Rvalue[ret].insert(var);
	return ret;
}

//为目标变量分配寄存器
string ObjectCodeGenerator::getReg() {
	//i: A:=B op C
	//如果B的现行值在某个寄存器Ri中，RVALUE[Ri]中只包含B
	//此外，或者B与A是同一个标识符或者B的现行值在执行四元式A:=B op C之后不会再引用
	//则选取Ri为所需要的寄存器R

	//如果src1不是数字
	if (!isNum(nowQuatenary->q.src1)) {
		//遍历src1所在的寄存器
		set<string>&src1pos = Avalue[nowQuatenary->q.src1];
		for (set<string>::iterator iter = src1pos.begin(); iter != src1pos.end(); iter++) {
			if ((*iter)[0] == '$') {
				if (Rvalue[*iter].size() == 1) {//如果该寄存器中值仅仅存有src1
					if (nowQuatenary->q.des == nowQuatenary->q.src1 || !nowQuatenary->info1.active) {//如果A,B是同一标识符或B以后不活跃
						Avalue[nowQuatenary->q.des].insert(*iter);
						Rvalue[*iter].insert(nowQuatenary->q.des);
						return *iter;
					}
				}
			}
		}
	}

	//为目标变量分配可能不正确
	//return allocateReg(nowQuatenary->q.des);
	string ret = allocateReg();
	Avalue[nowQuatenary->q.des].insert(ret);
	Rvalue[ret].insert(nowQuatenary->q.des);
	return ret;
}

void ObjectCodeGenerator::analyseBlock(map<string, vector<Block> >*funcBlocks) {
	for (map<string, vector<Block> >::iterator fbiter = funcBlocks->begin(); fbiter != funcBlocks->end(); fbiter++) {
		vector<IBlock> iBlocks;
		vector<Block>& blocks = fbiter->second;
		vector<set<string> >INL, OUTL, DEF, USE;

		//活跃变量的数据流方程
		//确定DEF，USE
		for (vector<Block>::iterator biter = blocks.begin(); biter != blocks.end(); biter++) {
			set<string>def, use;
			for (vector<Quaternary>::iterator citer = biter->codes.begin(); citer != biter->codes.end(); citer++) {
				if (citer->op == "j" || citer->op == "call") {
					//pass
				}
				else if (citer->op[0] == 'j') {//j>= j<=,j==,j!=,j>,j<
					if (isVar(citer->src1) && def.count(citer->src1) == 0) {//如果源操作数1还没有被定值
						use.insert(citer->src1);
					}
					if (isVar(citer->src2) && def.count(citer->src2) == 0) {//如果源操作数2还没有被定值
						use.insert(citer->src2);
					}
				}
				else {
					if (isVar(citer->src1) && def.count(citer->src1) == 0) {//如果源操作数1还没有被定值
						use.insert(citer->src1);
					}
					if (isVar(citer->src2) && def.count(citer->src2) == 0) {//如果源操作数2还没有被定值
						use.insert(citer->src2);
					}
					if (isVar(citer->des) && use.count(citer->des) == 0) {//如果目的操作数还没有被引用
						def.insert(citer->des);
					}
				}
			}
			INL.push_back(use);
			DEF.push_back(def);
			USE.push_back(use);
			OUTL.push_back(set<string>());
		}

		//确定INL，OUTL
		bool change = true;
		while (change) {
			change = false;
			int blockIndex = 0;
			for (vector<Block>::iterator biter = blocks.begin(); biter != blocks.end(); biter++, blockIndex++) {
				int next1 = biter->next1;
				int next2 = biter->next2;
				if (next1 != -1) {
					for (set<string>::iterator inlIter = INL[next1].begin(); inlIter != INL[next1].end(); inlIter++) {
						if (OUTL[blockIndex].insert(*inlIter).second == true) {
							if (DEF[blockIndex].count(*inlIter) == 0) {
								INL[blockIndex].insert(*inlIter);
							}
							change = true;
						}
					}
				}
				if (next2 != -1) {
					for (set<string>::iterator inlIter = INL[next2].begin(); inlIter != INL[next2].end(); inlIter++) {
						if (OUTL[blockIndex].insert(*inlIter).second == true) {
							if (DEF[blockIndex].count(*inlIter) == 0) {
								INL[blockIndex].insert(*inlIter);
							}
							change = true;
						}
					}
				}
			}
		}
		funcOUTL[fbiter->first] = OUTL;
		funcINL[fbiter->first] = INL;

		for (vector<Block>::iterator iter = blocks.begin(); iter != blocks.end(); iter++) {
			IBlock iBlock;
			iBlock.next1 = iter->next1;
			iBlock.next2 = iter->next2;
			iBlock.name = iter->name;
			for (vector<Quaternary>::iterator qIter = iter->codes.begin(); qIter != iter->codes.end(); qIter++) {
				iBlock.codes.push_back(QuaternaryWithInfo(*qIter, VarInfomation(-1, false), VarInfomation(-1, false), VarInfomation(-1, false)));
			}
			iBlocks.push_back(iBlock);
		}

		vector<map<string, VarInfomation> > symTables;//每个基本块对应一张符号表
		//初始化符号表
		for (vector<Block>::iterator biter = blocks.begin(); biter != blocks.end(); biter++) {//遍历每一个基本块
			map<string, VarInfomation>symTable;
			for (vector<Quaternary>::iterator citer = biter->codes.begin(); citer != biter->codes.end(); citer++) {//遍历基本块中的每个四元式
				if (citer->op == "j" || citer->op == "call") {
					//pass
				}
				else if (citer->op[0] == 'j') {//j>= j<=,j==,j!=,j>,j<
					if (isVar(citer->src1)) {
						symTable[citer->src1] = VarInfomation{ -1,false };
					}
					if (isVar(citer->src2)) {
						symTable[citer->src2] = VarInfomation{ -1,false };
					}
				}
				else {
					if (isVar(citer->src1)) {
						symTable[citer->src1] = VarInfomation{ -1,false };
					}
					if (isVar(citer->src2)) {
						symTable[citer->src2] = VarInfomation{ -1,false };
					}
					if (isVar(citer->des)) {
						symTable[citer->des] = VarInfomation{ -1,false };
					}
				}
			}
			symTables.push_back(symTable);
		}
		int blockIndex = 0;
		for (vector<set<string> >::iterator iter = OUTL.begin(); iter != OUTL.end(); iter++, blockIndex++) {//遍历每个基本块的活跃变量表
			for (set<string>::iterator viter = iter->begin(); viter != iter->end(); viter++) {//遍历活跃变量表中的变量
				symTables[blockIndex][*viter] = VarInfomation{ -1,true };
			}

		}

		blockIndex = 0;
		//计算每个四元式的待用信息和活跃信息
		for (vector<IBlock>::iterator ibiter = iBlocks.begin(); ibiter != iBlocks.end(); ibiter++, blockIndex++) {//遍历每一个基本块
			int codeIndex = ibiter->codes.size() - 1;
			for (vector<QuaternaryWithInfo>::reverse_iterator citer = ibiter->codes.rbegin(); citer != ibiter->codes.rend(); citer++, codeIndex--) {//逆序遍历基本块中的代码
				if (citer->q.op == "j" || citer->q.op == "call") {
					//pass
				}
				else if (citer->q.op[0] == 'j') {//j>= j<=,j==,j!=,j>,j<
					if (isVar(citer->q.src1)) {
						citer->info1 = symTables[blockIndex][citer->q.src1];
						symTables[blockIndex][citer->q.src1] = VarInfomation{ codeIndex,true };
					}
					if (isVar(citer->q.src2)) {
						citer->info2 = symTables[blockIndex][citer->q.src2];
						symTables[blockIndex][citer->q.src2] = VarInfomation{ codeIndex,true };
					}
				}
				else {
					if (isVar(citer->q.src1)) {
						citer->info1 = symTables[blockIndex][citer->q.src1];
						symTables[blockIndex][citer->q.src1] = VarInfomation{ codeIndex,true };
					}
					if (isVar(citer->q.src2)) {
						citer->info2 = symTables[blockIndex][citer->q.src2];
						symTables[blockIndex][citer->q.src2] = VarInfomation{ codeIndex,true };
					}
					if (isVar(citer->q.des)) {
						citer->info3 = symTables[blockIndex][citer->q.des];
						symTables[blockIndex][citer->q.des] = VarInfomation{ -1,false };
					}
				}
			}
		}

		funcIBlocks[fbiter->first] = iBlocks;
	}
}

void ObjectCodeGenerator::outputIBlocks(ostream& out) {
	for (map<string, vector<IBlock> >::iterator iter = funcIBlocks.begin(); iter != funcIBlocks.end(); iter++) {
		out << "[" << iter->first << "]" << endl;
		for (vector<IBlock>::iterator bIter = iter->second.begin(); bIter != iter->second.end(); bIter++) {
			out << bIter->name << ":" << endl;
			for (vector<QuaternaryWithInfo>::iterator cIter = bIter->codes.begin(); cIter != bIter->codes.end(); cIter++) {
				out << "    ";
				cIter->output(out);
				out << endl;
			}
			out << "    " << "next1 = " << bIter->next1 << endl;
			out << "    " << "next2 = " << bIter->next2 << endl;
		}
		cout << endl;
	}
}

void ObjectCodeGenerator::outputIBlocks() {
	outputIBlocks(cout);
}

void ObjectCodeGenerator::outputIBlocks(const char* fileName) {
	ofstream fout;
	fout.open(fileName);
	if (!fout.is_open()) {
		cerr << "file " << fileName << " open error" << endl;
		return;
	}
	outputIBlocks(fout);

	fout.close();
}

void ObjectCodeGenerator::outputObjectCode(ostream& out) {
	for (vector<string>::iterator iter = objectCodes.begin(); iter != objectCodes.end(); iter++) {
		out << *iter << endl;
	}
}

void ObjectCodeGenerator::outputObjectCode() {
	outputObjectCode(cout);
}

void ObjectCodeGenerator::outputObjectCode(const char* fileName) {
	ofstream fout;
	fout.open(fileName);
	if (!fout.is_open()) {
		cerr << "file " << fileName << " open error" << endl;
		return;
	}
	outputObjectCode(fout);

	fout.close();
}

//基本块出口，将出口活跃变量保存在内存
void ObjectCodeGenerator::storeOutLiveVar(set<string>&outl) {
	for (set<string>::iterator oiter = outl.begin(); oiter != outl.end(); oiter++) {
		string reg;//活跃变量所在的寄存器名称
		bool inFlag = false;//活跃变量在内存中的标志
		for (set<string>::iterator aiter = Avalue[*oiter].begin(); aiter != Avalue[*oiter].end(); aiter++) {
			if ((*aiter)[0] != '$') {//该活跃变量已经存储在内存中
				inFlag = true;
				break;
			}
			else {
				reg = *aiter;
			}
		}
		if (!inFlag) {//如果该活跃变量不在内存中，则将reg中的var变量存入内存
			storeVar(reg, *oiter);
		}
	}
}

void ObjectCodeGenerator::generateCodeForQuatenary(int nowBaseBlockIndex, int &arg_num, int &par_num, list<pair<string, bool> > &par_list) {
	if (nowQuatenary->q.op[0] != 'j'&&nowQuatenary->q.op != "call") {
		if (isVar(nowQuatenary->q.src1) && Avalue[nowQuatenary->q.src1].empty()) {
			outputError(string("变量") + nowQuatenary->q.src1 + "在引用前未赋值");
		}
		if (isVar(nowQuatenary->q.src2) && Avalue[nowQuatenary->q.src2].empty()) {
			outputError(string("变量") + nowQuatenary->q.src2 + "在引用前未赋值");
		}
	}


	if (nowQuatenary->q.op == "j") {
		objectCodes.push_back(nowQuatenary->q.op + " " + nowQuatenary->q.des);
	}
	else if (nowQuatenary->q.op[0] == 'j') {//j>= j<=,j==,j!=,j>,j<
		string op;
		if (nowQuatenary->q.op == "j>=")
			op = "bge";
		else if (nowQuatenary->q.op == "j>")
			op = "bgt";
		else if (nowQuatenary->q.op == "j==")
			op = "beq";
		else if (nowQuatenary->q.op == "j!=")
			op = "bne";
		else if (nowQuatenary->q.op == "j<")
			op = "blt";
		else if (nowQuatenary->q.op == "j<=")
			op = "ble";
		string pos1 = allocateReg(nowQuatenary->q.src1);
		string pos2 = allocateReg(nowQuatenary->q.src2);
		objectCodes.push_back(op + " " + pos1 + " " + pos2 + " " + nowQuatenary->q.des);
		if (!nowQuatenary->info1.active) {
			releaseVar(nowQuatenary->q.src1);
		}
		if (!nowQuatenary->info2.active) {
			releaseVar(nowQuatenary->q.src2);
		}
	}
	else if (nowQuatenary->q.op == "par") {
		par_list.push_back(pair<string, bool>(nowQuatenary->q.src1, nowQuatenary->info1.active));
	}
	else if (nowQuatenary->q.op == "call") {
		/*将参数压栈*/
		for (list<pair<string, bool> >::iterator aiter = par_list.begin(); aiter != par_list.end(); aiter++) {
			string pos = allocateReg(aiter->first);
			objectCodes.push_back(string("sw ") + pos + " " + to_string(top + 4 * (++arg_num + 1)) + "($sp)");
			if (!aiter->second) {
				releaseVar(aiter->first);
			}
		}
		/*更新$sp*/
		objectCodes.push_back(string("sw $sp ") + to_string(top) + "($sp)");
		objectCodes.push_back(string("addi $sp $sp ") + to_string(top));

		/*跳转到对应函数*/
		objectCodes.push_back(string("jal ") + nowQuatenary->q.src1);

		/*恢复现场*/
		objectCodes.push_back(string("lw $sp 0($sp)"));
	}
	else if (nowQuatenary->q.op == "return") {
		if (isNum(nowQuatenary->q.src1)) {//返回值为数字
			objectCodes.push_back("addi $v0 $zero " + nowQuatenary->q.src1);
		}
		else if (isVar(nowQuatenary->q.src1)) {//返回值为变量
			set<string>::iterator piter = Avalue[nowQuatenary->q.src1].begin();
			if ((*piter)[0] == '$') {
				objectCodes.push_back(string("add $v0 $zero ") + *piter);
			}
			else {
				objectCodes.push_back(string("lw $v0 ") + to_string(varOffset[*piter]) + "($sp)");
			}
		}
		if (nowFunc == "main") {
			objectCodes.push_back("j end");
		}
		else {
			objectCodes.push_back("lw $ra 4($sp)");
			objectCodes.push_back("jr $ra");
		}
	}
	else if (nowQuatenary->q.op == "get") {
		//varOffset[nowQuatenary->q.src1] = 4 * (par_num++ + 2);
		varOffset[nowQuatenary->q.des] = top;
		top += 4;
		Avalue[nowQuatenary->q.des].insert(nowQuatenary->q.des);
	}
	else if (nowQuatenary->q.op == "=") {
		//Avalue[nowQuatenary->q.des] = set<string>();
		string src1Pos;
		if (nowQuatenary->q.src1 == "@RETURN_PLACE") {
			src1Pos = "$v0";
		}
		else {
			src1Pos = allocateReg(nowQuatenary->q.src1);
		}
		Rvalue[src1Pos].insert(nowQuatenary->q.des);
		Avalue[nowQuatenary->q.des].insert(src1Pos);
	}
	else {// + - * /
		string src1Pos = allocateReg(nowQuatenary->q.src1);
		string src2Pos = allocateReg(nowQuatenary->q.src2);
		string desPos = getReg();
		if (nowQuatenary->q.op == "+") {
			objectCodes.push_back(string("add ") + desPos + " " + src1Pos + " " + src2Pos);
		}
		else if (nowQuatenary->q.op == "-") {
			objectCodes.push_back(string("sub ") + desPos + " " + src1Pos + " " + src2Pos);
		}
		else if (nowQuatenary->q.op == "*") {
			objectCodes.push_back(string("mul ") + desPos + " " + src1Pos + " " + src2Pos);
		}
		else if (nowQuatenary->q.op == "/") {
			objectCodes.push_back(string("div ") + src1Pos + " " + src2Pos);
			objectCodes.push_back(string("mflo ") + desPos);
		}
		if (!nowQuatenary->info1.active) {
			releaseVar(nowQuatenary->q.src1);
		}
		if (!nowQuatenary->info2.active) {
			releaseVar(nowQuatenary->q.src2);
		}
	}
}

void ObjectCodeGenerator::generateCodeForBaseBlocks(int nowBaseBlockIndex) {
	int arg_num = 0;//par的实参个数
	int par_num = 0;//get的形参个数
	list<pair<string, bool> > par_list;//函数调用用到的实参集list<实参名,是否活跃>

	if (nowFunc == "program") {
		int a = 1;
	}

	Avalue.clear();
	Rvalue.clear();
	set<string>& inl = funcINL[nowFunc][nowBaseBlockIndex];
	for (set<string>::iterator iter = inl.begin(); iter != inl.end(); iter++) {
		Avalue[*iter].insert(*iter);
	}

	//初始化空闲寄存器
	freeReg.clear();
	for (int i = 0; i <= 7; i++) {
		freeReg.push_back(string("$s") + to_string(i));
	}

	objectCodes.push_back(nowIBlock->name + ":");
	if (nowBaseBlockIndex == 0) {
		if (nowFunc == "main") {
			top = 8;
		}
		else {
			objectCodes.push_back("sw $ra 4($sp)");//把返回地址压栈
			top = 8;
		}
	}

	for (vector<QuaternaryWithInfo>::iterator cIter = nowIBlock->codes.begin(); cIter != nowIBlock->codes.end(); cIter++) {//对基本块内的每一条语句
		nowQuatenary = cIter;
		//如果是基本块的最后一条语句
		if (cIter + 1 == nowIBlock->codes.end()) {
			//如果最后一条语句是控制语句，则先将出口活跃变量保存，再进行跳转(j,call,return)
			if (isControlOp(cIter->q.op)) {
				storeOutLiveVar(funcOUTL[nowFunc][nowBaseBlockIndex]);
				generateCodeForQuatenary(nowBaseBlockIndex, arg_num, par_num, par_list);
			}
			//如果最后一条语句不是控制语句（是赋值语句），则先计算，再将出口活跃变量保存
			else {
				generateCodeForQuatenary(nowBaseBlockIndex, arg_num, par_num, par_list);
				storeOutLiveVar(funcOUTL[nowFunc][nowBaseBlockIndex]);
			}
		}
		else {
			generateCodeForQuatenary(nowBaseBlockIndex, arg_num, par_num, par_list);
		}

	}
}

void ObjectCodeGenerator::generateCodeForFuncBlocks(map<string, vector<IBlock> >::iterator &fiter) {
	varOffset.clear();
	nowFunc = fiter->first;
	vector<IBlock>&iBlocks = fiter->second;
	for (vector<IBlock>::iterator iter = iBlocks.begin(); iter != iBlocks.end(); iter++) {//对每一个基本块
		nowIBlock = iter;
		generateCodeForBaseBlocks(nowIBlock - iBlocks.begin());
	}
}

void ObjectCodeGenerator::generateCode() {
	objectCodes.push_back("lui $sp,0x1001");
	objectCodes.push_back("j main");
	for (map<string, vector<IBlock> >::iterator fiter = funcIBlocks.begin(); fiter != funcIBlocks.end(); fiter++) {//对每一个函数块
		generateCodeForFuncBlocks(fiter);
	}
	objectCodes.push_back("end:");
}