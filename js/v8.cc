// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "system.h"

#include <v8.h>

#include "debug.h"

namespace v8 {

// Reads a file into a v8 string.
static Handle<String> ReadFile(const char *name)
{
    FILE *file = fopen(name, "rb");
    if (file == NULL)
	return Handle<String> ();

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);

    char *chars = new char[size + 1];
    chars[size] = '\0';
    for (int i = 0; i < size;) {
	int read = static_cast < int >(fread(&chars[i], 1, size - i, file));
	i += read;
    }
    fclose(file);
    Handle<String> result = String::New(chars, size);
    delete[]chars;
    return result;
}

// Extracts a C string from a V8 Utf8Value.
static const char *ToCString(const String::Utf8Value & value)
{
    return *value ? *value : "<string conversion failed>";
}

static void ReportException(TryCatch * try_catch)
{
    HandleScope handle_scope;
    String::Utf8Value exception(try_catch->Exception());
    const char *exception_string = ToCString(exception);
    Handle<Message> message = try_catch->Message();
    if (message.IsEmpty()) {
	// V8 didn't provide any extra information about this error; just
	// print the exception.
	printf("%s\n", exception_string);
    } else {
	// Print (filename):(line number): (message).
	String::Utf8Value filename(message->GetScriptResourceName());
	const char *filename_string = ToCString(filename);
	int linenum = message->GetLineNumber();
	printf("%s:%i: %s\n", filename_string, linenum, exception_string);
	// Print line of source code.
	String::Utf8Value sourceline(message->GetSourceLine());
	const char *sourceline_string = ToCString(sourceline);
	printf("%s\n", sourceline_string);
	// Print wavy underline (GetUnderline is deprecated).
	int start = message->GetStartColumn();
	for (int i = 0; i < start; i++) {
	    printf(" ");
	}
	int end = message->GetEndColumn();
	for (int i = start; i < end; i++) {
	    printf("^");
	}
	printf("\n");
	String::Utf8Value stack_trace(try_catch->StackTrace());
	if (stack_trace.length() > 0) {
	    const char *stack_trace_string = ToCString(stack_trace);
	    printf("%s\n", stack_trace_string);
	}
    }
}

// Executes a string within the current v8 context.
static bool ExecuteString(Handle<String> source,
		   Handle<Value> name,
		   bool print_result, bool report_exceptions)
{
    HandleScope handle_scope;
    TryCatch try_catch;
    Handle<Script> script = Script::Compile(source, name);
    if (script.IsEmpty()) {
	// Print errors that happened during compilation.
	if (report_exceptions)
	    ReportException(&try_catch);
	return false;
    } else {
	Handle<Value> result = script->Run();
	if (result.IsEmpty()) {
	    assert(try_catch.HasCaught());
	    // Print errors that happened during execution.
	    if (report_exceptions)
		ReportException(&try_catch);
	    return false;
	} else {
	    assert(!try_catch.HasCaught());
	    if (print_result && !result->IsUndefined()) {
		// If all went well and the result wasn't undefined then print
		// the returned value.
		String::Utf8Value str(result);
		const char *cstr = ToCString(str);
		printf("%s\n", cstr);
	    }
	    return true;
	}
    }
}

// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
static Handle<Value> ShellPrint(const Arguments & args)
{
    bool first = true;
    for (int i = 0; i < args.Length(); i++) {
	HandleScope handle_scope;
	if (first) {
	    first = false;
	} else {
	    printf(" ");
	}
	String::Utf8Value str(args[i]);
	const char *cstr = ToCString(str);
	printf("%s", cstr);
    }
    printf("\n");
    fflush(stdout);
    return Undefined();
}

// The callback that is invoked by v8 whenever the JavaScript 'read'
// function is called.  This function loads the content of the file named in
// the argument into a JavaScript string.
static Handle<Value> ShellRead(const Arguments & args)
{
    if (args.Length() != 1) {
	return ThrowException(String::New("Bad parameters"));
    }
    String::Utf8Value file(args[0]);
    if (*file == NULL) {
	return ThrowException(String::New("Error loading file"));
    }
    Handle<String> source = ReadFile(*file);
    if (source.IsEmpty()) {
	return ThrowException(String::New("Error loading file"));
    }
    return source;
}

// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
static Handle<Value> ShellLoad(const Arguments & args)
{
    for (int i = 0; i < args.Length(); i++) {
	HandleScope handle_scope;
	String::Utf8Value file(args[i]);
	if (*file == NULL) {
	    return ThrowException(String::New("Error loading file"));
	}
	Handle<String> source = ReadFile(*file);
	if (source.IsEmpty()) {
	    return ThrowException(String::New("Error loading file"));
	}
	if (!ExecuteString(source, String::New(*file), false, false)) {
	    return ThrowException(String::New("Error executing file"));
	}
    }
    return Undefined();
}

// The callback that is invoked by v8 whenever the JavaScript 'quit'
// function is called.  Quits.
static Handle<Value> ShellQuit(const Arguments & args)
{
    // If not arguments are given args[0] will yield undefined which
    // converts to the integer value 0.
    int exit_code = args[0]->Int32Value();
    fflush(stdout);
    fflush(stderr);
    exit(exit_code);
    return Undefined();
}

static Handle<Value> ShellVersion(const Arguments & args)
{
    return String::New(V8::GetVersion());
}

// Creates a new execution environment containing the built-in
// functions.
static Persistent<Context> CreateShellContext()
{
    Handle<ObjectTemplate> global = ObjectTemplate::New();
    global->Set(String::New("print"), FunctionTemplate::New(ShellPrint));
    global->Set(String::New("read"), FunctionTemplate::New(ShellRead));
    global->Set(String::New("load"), FunctionTemplate::New(ShellLoad));
    global->Set(String::New("quit"), FunctionTemplate::New(ShellQuit));
    global->Set(String::New("version"), FunctionTemplate::New(ShellVersion));
    return Context::New(NULL, global);
}

static bool run_shell;

// Process remaining command line arguments and execute files
static int RunMain(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
	const char *str = argv[i];
	if (strcmp(str, "--shell") == 0) {
	    run_shell = true;
	} else if (strcmp(str, "-f") == 0) {
	    // Ignore any -f flags for compatibility with the other stand-
	    // alone JavaScript engines.
	    continue;
	} else if (strncmp(str, "--", 2) == 0) {
	    printf("Warning: unknown flag %s.\nTry --help for options\n",
		   str);
	} else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
	    // Execute argument given to -e option directly.
	    Handle<String> file_name = String::New("unnamed");
	    Handle<String> source = String::New(argv[++i]);
	    if (!ExecuteString(source, file_name, false, true))
		return 1;
	} else {
	    // Use all other arguments as names of files to load and run.
	    Handle<String> file_name = String::New(str);
	    Handle<String> source = ReadFile(str);
	    if (source.IsEmpty()) {
		printf("Error reading '%s'\n", str);
		continue;
	    }
	    if (!ExecuteString(source, file_name, false, true))
		return 1;
	}
    }
    return 0;
}

// The read-eval-execute loop of the shell.
static void RunShell(Handle<Context> context)
{
    printf("V8 version %s [sample shell]\n", V8::GetVersion());
    static const int kBufferSize = 256;
    // Enter the execution environment before evaluating any code.
    Context::Scope context_scope(context);
    Local<String> name(String::New("(shell)"));
    while (true) {
	char buffer[kBufferSize];
	printf("> ");
	char *str = fgets(buffer, kBufferSize, stdin);
	if (str == NULL)
	    break;
	HandleScope handle_scope;
	ExecuteString(String::New(str), name, true, true);
    }
    printf("\n");
}

int ShellMain(int argc, char *argv[])
{
    V8::SetFlagsFromCommandLine(&argc, argv, true);
    run_shell = (argc == 1);
    int result;
    {
	HandleScope handle_scope;
	Persistent<Context> context = CreateShellContext();
	if (context.IsEmpty()) {
	    printf("Error creating context\n");
	    return 1;
	}
	context->Enter();
	result = RunMain(argc, argv);
	if (run_shell)
	    RunShell(context);
	context->Exit();
	context.Dispose();
    }
    V8::Dispose();
    return result;
}

} // namespace v8

int main(int argc, char *argv[])
{
    return v8::ShellMain(argc, argv);
}
