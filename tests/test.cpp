#include <gtest/gtest.h>

#include "refactor_tool.h"
#include "clang/Tooling/ArgumentsAdjusters.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

using namespace llvm;

using namespace std::string_literals;

#define TO_C_STRING_IMPL(x) #x
#define TO_C_STRING(x) TO_C_STRING_IMPL(x)

static cl::OptionCategory tool_category("refactor-tool-tester options");
const static std::string temp_file = ".\\tests_data\\temp"s;

std::string ReadFile(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs && !ifs.is_open()) return {};

    std::stringstream buffer;
    buffer << ifs.rdbuf();

    return buffer.str();
}

void WriteFile(const std::string& text, 
    const std::string& filename)
{
    std::ofstream ofs(filename);
    ofs << text;
}

std::string RefactorFile(const std::string& filename)
{
    // Эмуляция входящих параметров программы
    int argc = 2;
    const char* argv[] = { "test.exe", 
        filename.c_str() };

    Expected expected_parser = 
        CommonOptionsParser::create(argc, argv, 
            tool_category);
    
    CommonOptionsParser& options_parser = 
        expected_parser.get();

    // Создаем ClangTool
    ClangTool tool(options_parser.getCompilations(), 
        options_parser.getSourcePathList());
    
    // Настройка аргументов «из командной строки»
    std::vector<std::string> args = {
        "-x", "c++", 
        "-resource-dir", TO_C_STRING(CLANG_RESOURCE_DIR),
        "-stdlib=libc++",
        "-I", TO_C_STRING(CLANG_INCLUDE_DIR)
    };

    tool.appendArgumentsAdjuster(getClangStripOutputAdjuster());
    tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(args, 
        ArgumentInsertPosition::BEGIN));

    // Запускаем RefactorAction
    tool.run(newFrontendActionFactory<CodeRefactorAction>().get());

    std::ifstream ifs(temp_file);
    if (!ifs && !ifs.is_open()) return {};

    std::stringstream buffer;
    buffer << ifs.rdbuf();

    return buffer.str();
}

struct RefactoringTestFixture : ::testing::TestWithParam<unsigned>
{};

TEST_P(RefactoringTestFixture, VariousScenarios)
{
    const std::string input_file = ".\\tests_data\\_test" + std::to_string(GetParam());
    const std::string reference_file = input_file + "_ref";
    
    std::string contents = ReadFile(input_file);
    ASSERT_TRUE(contents.size());

    WriteFile(contents, temp_file);
    ASSERT_TRUE(RefactorFile(temp_file) == 
        ReadFile(reference_file));
}

INSTANTIATE_TEST_SUITE_P(RefactoringTool_Tests, 
    RefactoringTestFixture, 
    ::testing::Values(1, 2, 3, 4, 5, 6, 7));