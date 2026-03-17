#include "refactor_tool.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

using namespace llvm;

static cl::OptionCategory tool_category("refactor-tool options");

#define TO_C_STRING_IMPL(x) #x
#define TO_C_STRING(x) TO_C_STRING_IMPL(x)

int main(int argc, const char* argv[])
{
    // Парсер опций: обрабатывает флаги командной строки, компиляционные базы данных.
    Expected ExpectedParser = 
        CommonOptionsParser::create(argc, argv, 
            tool_category);
    
    if (!ExpectedParser)
    {
        errs() << ExpectedParser.takeError();
        return EXIT_FAILURE;
    }

    CommonOptionsParser& OptionsParser = ExpectedParser.get();
    
    // Создаем ClangTool
    ClangTool tool(OptionsParser.getCompilations(), 
        OptionsParser.getSourcePathList());
    
#ifdef REFACTOR_DEBUG
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
#endif

    // Запускаем RefactorAction
    return tool.run(newFrontendActionFactory<CodeRefactorAction>().get());
}