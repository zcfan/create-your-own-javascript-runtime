# 定制 javascript runtime - Part 2：构建依赖

这一 part 我们从源码构建一下 v8 和 libuv 两个依赖项，算是上一 part 知识的实战演练。

> 注意：作者不是 c/c++ 开发者，构建命令、范例代码都是边查边写，主打不求甚解。**绝对**不要信赖本文中的任何命令、代码，对参考本文内容导致的任何损失不负责任。

## 1 编译 v8

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

## 2 编译 libuv

```bash
# 下载 libuv 源码
git clone https://github.com/libuv/libuv.git
cd libuv

# 使用 cmake 构建
mkdir -p build
(cd build && cmake ..)
cmake --build build

# 运行测试验证构建是否成功
build/uv_run_tests_a
```

构建完成后，`build/libuv.a` 即为 libuv 的静态库。同目录还有一个 `libuv.1.0.0.dylib` 这是动态库，我们后面使用静态库就好。

## 3 扩展 v8/samples/shell.cc

我们后面的例子在 shell.cc 这个范例的基础上扩展，现在先体验一下这个范例：

```bash
g++ -I. -Iinclude samples/shell.cc -o shell -fno-rtti -lv8_monolith -lv8_libbase -lv8_libplatform -ldl -Lout.gn/x64.release.sample/obj/ -pthread -std=c++17 -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX

./shell
V8 version 11.9.169.7 [sample shell]
> 1 + 1
2
> 'hello'
hello
> print('123')
123
> print('123',123,'abc')
123 123 abc
> load('loadme.js')
# loadme.js 需要把 demo/v8-sample/loadme.js 复制到 demo/v8 中
load and executed! 2
```

无论 `print` 还是 `load` 函数，都是常规的 javascript runtime 中没有的：

```bash
$ node
Welcome to Node.js v18.1.0.
Type ".help" for more information.
> print
Uncaught ReferenceError: print is not defined
> load
Uncaught ReferenceError: load is not defined
```

因为它们是在 `samples/shell.cc` 中自定义的暴露给 js 环境的函数。所以现在这个名为 `shell` 的可执行文件完全可以算是我们自己定制的 javascript runtime 了。

`shell.cc` 的 `main` 结构和前面看过的 `hello_world.cc` 很像:

```c++
int main(int argc, char* argv[]) {
  //==========
  // 初始化
  //==========

  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::V8::Initialize();
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  //==========
  // 主体逻辑
  //==========

  // 如果调用 shell 时指定了参数，比如 `shell loadme.js`，则 argc == 2（因为第一个参数是可执行文件的名字 shell）
  // 这种情况下行为是执行参数指定的 javascript 脚本
  // 如果没有指定参数，直接执行 `shell`，则 argc == 1，run_shell 为 true，
  // 这种情况下行为是打开一个可交互的 shell
  run_shell = (argc == 1);
  int result;
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = CreateShellContext(isolate);
    if (context.IsEmpty()) {
      fprintf(stderr, "Error creating context\n");
      return 1;
    }
    v8::Context::Scope context_scope(context);
    // run_shell == false 的分支（argc == 1，RunMain 内部会直接 return）
    result = RunMain(isolate, platform.get(), argc, argv);
    // run_shell == true 的分支
    if (run_shell) RunShell(context, platform.get());
  }

  //==========
  // 销毁
  //==========

  isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  delete create_params.array_buffer_allocator;
  return result;
}
```

前面用到的 `print` `load` 这些函数是在 `CreateShellContext` 中配置的：

```c++
// Creates a new execution environment containing the built-in
// functions.
v8::Local<v8::Context> CreateShellContext(v8::Isolate* isolate) {
  // 在我们的 javascript runtime 中创建一个 object
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  // 绑定下面的 Print 函数到这个 object 的 print 字段
  global->Set(isolate, "print", v8::FunctionTemplate::New(isolate, Print));
  global->Set(isolate, "read", v8::FunctionTemplate::New(isolate, Read));
  global->Set(isolate, "load", v8::FunctionTemplate::New(isolate, Load));
  global->Set(isolate, "quit", v8::FunctionTemplate::New(isolate, Quit));
  global->Set(isolate, "version", v8::FunctionTemplate::New(isolate, Version));
  // 创建 Context，将前面创建的 object 传入第三个参数（global_template）
  // 它的字段（print, read, load ...）就会暴露在 javascript 的 global 中。
  return v8::Context::New(isolate, NULL, global);
}

// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
void Print(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool first = true;
  for (int i = 0; i < info.Length(); i++) {
    v8::HandleScope handle_scope(info.GetIsolate());
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    v8::String::Utf8Value str(info.GetIsolate(), info[i]);
    const char* cstr = ToCString(str);
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
}
```

下一步我们的目标就是在这个范例的基础上扩展我们熟悉的 `setTimeout` 函数，在我们这个*极简* runtime 中 `setTimeout` 也不存在：

```bash
> setTimeout
(shell):1: ReferenceError: setTimeout is not defined
setTimeout
^
ReferenceError: setTimeout is not defined
    at (shell):1:1
```

编辑 `shell.cc`，或者直接使用 `demo/v8-sample/shell.cc`：

```c++
// line 43: include libuv 的头文件
#include "uv.h"

// line 45: 定义 timer 结构体，用于实现 Timeout
struct timer 
{
    uv_timer_t uvTimer;
    v8::Isolate *isolate;
    v8::Global<v8::Function> callback;
};

// line 51: 初始化 libuv 的 timer
uv_loop_t *loop = uv_default_loop();

// line 73: 声明 Timeout 和 onTimerCallback 两个函数
// Timeout 是要暴露到 js global 的函数
// onTimerCallback 是 libuv timer 时间到了之后调用的 callback
void Timeout(const v8::FunctionCallbackInfo<v8::Value>& info);
void onTimerCallback(uv_timer_t *handle);

// line 121: 在 CreateShellContext 中向 global 注册 setTimeout
v8::Local<v8::Context> CreateShellContext(v8::Isolate* isolate) {
  // ...

  global->Set(isolate, "print", v8::FunctionTemplate::New(isolate, Print));
  global->Set(isolate, "read", v8::FunctionTemplate::New(isolate, Read));
  global->Set(isolate, "load", v8::FunctionTemplate::New(isolate, Load));
  global->Set(isolate, "quit", v8::FunctionTemplate::New(isolate, Quit));
  global->Set(isolate, "version", v8::FunctionTemplate::New(isolate, Version));
  // 增加这一行
  global->Set(isolate, "setTimeout", v8::FunctionTemplate::New(isolate, Timeout));

  // ...
}

// line 225: 实现 Timeout 函数
void Timeout(const v8::FunctionCallbackInfo<v8::Value>& info) {
  // libuv 的概念
  uv_loop_t *loop = uv_default_loop();
  auto isolate = info.GetIsolate();
  auto context = isolate->GetCurrentContext();
  // 获得 setTimeout 第一个参数
  v8::Local<v8::Value> callback = info[0];
  // 获得 setTimeout 第二个参数
  int64_t sleep = info[1]->IntegerValue(context).ToChecked();

  // 确保第一个参数是个函数
  if(!callback->IsFunction()) 
  {
    printf("callback not declared!");
    return;
  }

  // 初始化 timer 结构体
  timer *timerWrap = new timer();
  timerWrap->callback.Reset(isolate, callback.As<v8::Function>());
  timerWrap->uvTimer.data = (void *)timerWrap;
  timerWrap->isolate = isolate;

  // 设置 libuv timer
  uv_timer_init(loop, &timerWrap->uvTimer);
  uv_timer_start(&timerWrap->uvTimer, onTimerCallback, sleep, 0);
  uv_run(loop, UV_RUN_DEFAULT);
}

// line 254: 实现 onTimerCallback
void onTimerCallback(uv_timer_t *handle) {
  // 获取 timer 结构体
  timer *timerWrap = (timer *)handle->data;
  v8::Isolate *isolate = timerWrap->isolate;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if(isolate->IsDead()) {
    printf("isolate is dead");
    return;
  }

  v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(
    isolate,
    timerWrap->callback
  );
  v8::Local<v8::Value> result;
  v8::Handle<v8::Value> resultr [] = {};

  // 调用 callback
  if(callback->Call(
    context,
    context->Global(),
    0,
    resultr).ToLocal(&result)
  ) {
    // OK, the callback succeeded
  } else {
    // some exception happened
  }
}
```

构建命令要对应修改一下，把 `libuv.a` 连接进来：

```bash
g++ -I. -Iinclude -I../libuv/include samples/shell.cc ../libuv/build/libuv.a -o shell -fno-rtti -lv8_monolith -lv8_libbase -lv8_libplatform -ldl -Lout.gn/x64.release.sample/obj/ -pthread -std=c++17 -DV8_COMPRESS_POINTERS -DV8_ENABLE_SANDBOX
# 增加了这两段
# -I../libuv/include
# ../libuv/build/libuv.a
```

构建完成，体验一下我们全新的 `setTimeout`：

```bash
$ ./shell
V8 version 11.9.169.7 [sample shell]
> setTimeout(() => { print(123) }, 1000)
123
```

## 4 总结

这一 part 中我们从源码构建了 v8 和 libuv 的静态库，并在 `v8/samples/shell.cc` 的基础上扩展了 `setTimeout` api。尽管非常简单粗暴，bug 满天飞，但我们的确定制了一个独一无二的 javascript runtime。

后面我们再尝试在这个基础上去扩展一些更加有趣的特性。

## 5 扩展阅读

我们已经多次遇到了 Isolate、Context、loop 这些 v8、libuv 的概念，但我这里并不打算展开，我们现阶段只要知道怎么照搬，并且对它们的作用有个大概感受即可。

有精力想深入了解的可以阅读这篇文档：

https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/bindings/core/v8/V8BindingDesign.md

这里还有一篇 cloudflare 如何使用 v8 Isolate 实现 Cloudflare Worker 产品的文章，性能相对于之前的 serverless 方案有大幅提升：

https://blog.cloudflare.com/serverless-performance-comparison-workers-lambda/

## 6 参考/翻译

- https://github.com/ErickWendel/myownnode
- https://v8.dev/docs/source-code#using-git
- https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/bindings/core/v8/V8BindingDesign.md