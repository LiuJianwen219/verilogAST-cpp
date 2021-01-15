# 一些说明
```c++
std::move(t)
// 用来拷贝对象
```
```c++
std::unique_ptr
// 是一种独占的智能指针，禁止与其他智能指针共享同一个对象，保证代码安全
// 但是可以用std::move 来转移数据
```
```c++
std::variant
// 类似C语言的union，但是是可辨识的类型安全的联合类型
```

MSB（Most Significant Bit）：最高有效位，二进制中代表最高值的比特位，这一位对数值的影响最大。

LSB（Least Significant Bit）：最低有效位，二进制中代表最低值的比特位

目前项目存在的一些TODO:
1.  模块的端口只支持单一的方式指定 
2.  InlineVerilog句子的合法性没有进行验证
3.  模块实例化的时候，没有进行传入参数是否是模块真正端口子集的检查