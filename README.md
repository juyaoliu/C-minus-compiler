# C-minus-compiler
同济大学编译原理课程设计类C编译器任务，实现了过程调用，生成mips汇编代码

## 词法分析器接口说明
初始化词法分析器对象，读取程序
```C++
LexicalAnalyser lexicalAnalyser(C_minus_program_name);
```
对程序进行词法分析
```C++
lexicalAnalyser.analyse();
```

得到词法分析结果
```C++
lexicalAnalyser.getResult()
```

将词法分析结果输出到标准输出
```C++
lexicalAnalyser.outputToScreen()
```

将词法分析结果输出到文件
```C++
lexicalAnalyser.outputToFile(fileName);
```

## 语法及语意分析器接口说明
初始语法及语意分析器并构造DFA
```C++
ParserAndSemanticAnalyser parserAndSemanticAnalyser("productions.txt");
```

将构造的DFA输出到标准输出
```C++
parserAndSemanticAnalyser.outputDFA();
```

将构造的DFA输出到文件
```C++
parserAndSemanticAnalyser.outputDFA(output_file_name);
```

将中间代码输出到标准输出
```C++
parserAndSemanticAnalyser.outputIntermediateCode();
```

将中间代码输出到文件
```C++
parserAndSemanticAnalyser.outputIntermediateCode(output_file_name);
```

根据词法分析结果进行语法及语意分析，并将分析结果输出到标准输出
```C++
parserAndSemanticAnalyser.analyse(lexicalAnalyser.getResult());
```

根据词法分析结果进行语法及语意分析，并将分析结果输出到文件
```C++
parserAndSemanticAnalyser.analyse(lexicalAnalyser.getResult(), output_file_name);
```

得到生成的中间代码中各个函数入口地址
```C++
parserAndSemanticAnalyser.getFuncEnter;
```

得到语法及语意分析结果
```C++
parserAndSemanticAnalyser.getIntermediateCode();
```

## 中间代码接口说明
将中间代码输出到屏幕
```C++
code.output();
```

将中间代码输出到文件
```C++
code.output(output_file_name);
```

划分基本块
```C++
code.divideBlocks(parserAndSemanticAnalyser.getFuncEnter());
```

将基本块输出到标准输出
```C++
code.outputBlocks();
```

将基本块输出到文件
```C++
code.outputBlocks(output_file_name);
```

返回基本块划分结果
```C++
code.getFuncBlock();
```

## 目标代码生成器接口说明
分析基本块的待用/活跃信息，确定出口活跃变量和入口活跃变量
```C++
objectCodeGenerator.analyseBlock(code->getFuncBlock())；
```

输出基本块及待用/活跃信息到标准输出
```C++
objectCodeGenerator.outputIBlocks();
```

生成目标代码
```C++
objectCodeGenerator.generateCode();
```

输出目标代码到标准输出
```C++
objectCodeGenerator.outputObjectCode();
```

输出目标代码到文件
```C++
objectCodeGenerator.outputObjectCode(asm_name);
```
