# 定制 javascript runtime - Part 2：构建依赖

## 1 准备依赖

这一 part 我们从源码构建一下 v8 和 libuv 两个依赖项，算是上一 part 知识的实战演练。

### 1.1 编译 v8

```bash
cd demo

# 下载编译 v8 所需的 depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

# 配置环境变量
export PATH=/path/to/depot_tools:$PATH

# clone v8 引擎源码
# 注意：耗时操作，几分钟没动静是正常的，网络没问题也慢，先放着去搞杯咖啡吧
fetch v8
cd v8

# 为了保证后面的范例能正常运行，checkout 文档指定的特定版本
git checkout branch-heads/11.9 -b sample -t

# 创建构建配置
tools/dev/v8gen.py x64.release.sample

# 构建 v8 静态库
# 注意：耗时操作，看电脑性能，十几分钟或者几十分钟
ninja -C out.gn/x64.release.sample v8_monolith
# -C out.gn/x64.release.sample：构建前先 cd 到 out.gn/x64.release.sample 这个路径

# 构建范例程序
g++ -I. -Iinclude samples/hello-world.cc -o hello_world -fno-rtti -lv8_monolith -lv8_libbase -lv8_libplatform -ldl -Lout.gn/x64.release.sample/obj/ -pthread -std=c++17 -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX
# 这里有大量选项，如果 part1 都看明白了应该不至于听天书
# 我们调泛用的选项说明一下，牵扯其他细节太多的选项就跳过：
# g++：在 mac 下和 part1 中的 gcc 是一回事都是 clang，可以这样验证 `gcc -v` `g++ -v` 的输出结果是一样的
# -I. -Iinclude：把当前目录（out.gn/x64.release.sample）以及当前目录下的 include 目录都增加到 include 查找路径中
# samples/hello-world.cc：要构建的源代码
# -o hello_world：构建产物（可执行文件）命名为 hello_world
# -fno-rtti：照抄，略
# -lv8_monolith：将前一步构建出的 libv8_monolith 静态库连接到构建的可执行文件中
# -lv8_libbase：同上
# -lv8_libplatform：同上
# -ldl：同上
# -Lout.gn/x64.release.sample/obj/：将 out.gn/x64.release.sample/obj/ 目录添加到库查找路径，前面用 -l 指定要连接的库都在这个路径中
# -pthread：照抄，略
# -std=c++17：照抄，略
# -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX：照抄，略

# 复制 icudtl.dat 到可执行文件所在目录
cp out.gn/x64.release.sample/icudtl.dat .
# icu 是 International Components for Unicode，Unicode 和全球化支持库
# https://icu.unicode.org/#h.i33fakvpjb7o
# v8 使用了 icu 库，icudtl.dat 是 icu 库所需的 Unicode 数据，默认从可执行文件的所在目录获取

# 执行可执行程序
./hello_world
# output:
# Hello, World!
# 3 + 4 = 7
```

至此我们就借助 v8 库，编译运行了一个简单的 hello_world 范例程序。我们可以一起看看  `samples/hello-world.cc`  范例是怎么实现的。不要在意细节，先对 v8 的使用有个印象：

```c++
// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"

int main(int argc, char* argv[]) {
  // Initialize V8. 初始化
  v8::V8::InitializeICUDefaultLocation(argv[0]); // 从默认位置初始化 ICU（前面复制的 icudtl.dat）
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  // Create a new Isolate and make it the current one.
  // Isolate 是 v8 的概念，暂时留个印象就好
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    v8::HandleScope handle_scope(isolate);

    // Create a new context.
    // 同样的，留个印象
    v8::Local<v8::Context> context = v8::Context::New(isolate);

    // Enter the context for compiling and running the hello world script.
    v8::Context::Scope context_scope(context);

    // 第一个例子，执行 `'hello' + ', World'` 并将执行结果 printf 出来
    {
      // Create a string containing the JavaScript source code.
      // 创建一个包含了 js 源码的字符串
      v8::Local<v8::String> source =
          v8::String::NewFromUtf8Literal(isolate, "'Hello' + ', World!'");

      // Compile the source code.
      // 编译源码
      v8::Local<v8::Script> script =
          v8::Script::Compile(context, source).ToLocalChecked();

      // Run the script to get the result.
      // 运行脚本，获得结果
      v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

      // Convert the result to an UTF8 string and print it.
      // 将结果转换为 utf8 字符串，并打印
      v8::String::Utf8Value utf8(isolate, result);
      printf("%s\n", *utf8);
    }

    // 第二个例子，演示 wasm 实现的 add 函数，执行结果返回并 printf
    {
      // Use the JavaScript API to generate a WebAssembly module.
      //
      // |bytes| contains the binary format for the following module:
      //
      //     (func (export "add") (param i32 i32) (result i32)
      //       get_local 0
      //       get_local 1
      //       i32.add)
      //
      const char csource[] = R"(    
	      // 这段二进制是前面注释里的源码编译而来
        let bytes = new Uint8Array([
          0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
          0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
          0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
          0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
        ]);
        let module = new WebAssembly.Module(bytes);
        let instance = new WebAssembly.Instance(module);
        instance.exports.add(3, 4);
      )";

      // Create a string containing the JavaScript source code.
      v8::Local<v8::String> source =
          v8::String::NewFromUtf8Literal(isolate, csource);

      // Compile the source code.
      v8::Local<v8::Script> script =
          v8::Script::Compile(context, source).ToLocalChecked();

      // Run the script to get the result.
      v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

      // Convert the result to a uint32 and print it.
      uint32_t number = result->Uint32Value(context).ToChecked();
      printf("3 + 4 = %u\n", number);
    }
    
    // 可以复制前面的两段代码，在这里自己修改一下摸索摸索，比如这里我修改第一个例子，试试 Template literals 语法
    {
      const char csource[] = R"(
        const a = 'hello world';
        const b = 'from javascript!';
        const c = 3.1415926;
        `${a} ${b} ${c}\n`;
      )";

      // Create a string containing the JavaScript source code.
      v8::Local<v8::String> source =
          v8::String::NewFromUtf8Literal(isolate, csource);

      // Compile the source code.
      v8::Local<v8::Script> script =
          v8::Script::Compile(context, source).ToLocalChecked();

      // Run the script to get the result.
      v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

      v8::String::Utf8Value utf8(isolate, result);
      printf("js result: %s", *utf8);
    }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  delete create_params.array_buffer_allocator;
  return 0;
}
```

如果修改了范例代码想要摸索一下，需要再次执行前面的“构建范例程序”所使用的命令，然后 `./hello_world` 。

另外，samples 中还有两个例子：

- `process.cc`：演示如何实现一个可以用 js 扩展的 http 请求处理程序，js 可以提供一个 Process 函数来扩展 http 请求处理程序的功能，比如可以统计每个页面的被访问次数等。
- `shell.cc`：接收文件名作为参数，读取并执行这些文件的内容。还有命令提示符，可以输入 js 代码片段并执行。同时给 js 提供了一个名为 `print` 的函数

### 1.2 libuv

```bash
# 下载 libuv 源码
git clone https://github.com/libuv/libuv.git
cd libuv

# 使用 cmake 构建
mkdir -p build
(cd build && cmake ..)
cmake --build build

# 验证构建是否成功
build/uv_run_tests_a
```

构建完成后，`build/libuv.a` 即为 libuv 的静态库。同目录还有一个 `libuv.1.0.0.dylib` 这是动态库，我们后面使用静态库就好。





## 参考/翻译

https://github.com/ErickWendel/myownnode

https://v8.dev/docs/source-code#using-git