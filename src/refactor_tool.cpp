#include "refactor_tool.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

using namespace llvm;

static cl::OptionCategory tool_category("refactor-tool options");

// Метод run вызывается для каждого совпадения с матчем. 
// Мы проверяем тип совпадения по bind-именам и применяем рефакторинг.
void RefactorHandler::run(const MatchFinder::MatchResult& result)
{
    DiagnosticsEngine& diag = result.Context->getDiagnostics();
    ASTContext& ctx = *result.Context; // Получаем SourceManager для проверки isInMainFile
    
    if (const CXXDestructorDecl* Dtor = 
            result.Nodes.getNodeAs<CXXDestructorDecl>("nonVirtualDtor"))
    {
        if (nonvirt_dtors_.contains((uint64_t)Dtor)) return;
        
        nonvirt_dtors_.emplace((uint64_t)Dtor);
        handle_nv_dtor(Dtor, diag, ctx);
    }

    if (const CXXMethodDecl* Method = 
            result.Nodes.getNodeAs<CXXMethodDecl>("missingOverride"))
    {
        handle_miss_override(Method, diag, ctx);
    }

    if (const VarDecl* LoopVar = 
            result.Nodes.getNodeAs<VarDecl>("nonRefLoopVarDecl"))
    {
        handle_crange_for(LoopVar, diag, ctx);
    }
}

//todo: необходимо реализовать обработку случая невиртуального деструктора
void RefactorHandler::handle_nv_dtor(const CXXDestructorDecl* Dtor, 
    DiagnosticsEngine& diag, ASTContext& ctx)
{
    //Реализуйте Ваш код ниже
    const unsigned DiagID = 
        diag.getCustomDiagID(DiagnosticsEngine::Remark,
            "Объявлен невиртуальный деструктор у наследуемой сущности");
    diag.Report(Dtor->getLocation(), DiagID);

    SourceLocation target = 
        Lexer::GetBeginningOfToken(Dtor->getBeginLoc(),
            ctx.getSourceManager(), ctx.getLangOpts());
    rewriter_.InsertTextBefore(target, "virtual ");
}

// Обработка случая отсутствия override
void RefactorHandler::handle_miss_override(const CXXMethodDecl* Method, 
    DiagnosticsEngine& diag, ASTContext& ctx)
{
    //Реализуйте Ваш код ниже
    const unsigned DiagID = 
        diag.getCustomDiagID(DiagnosticsEngine::Remark,
            "Найден унаследованный метод без ключевого слова override");
    diag.Report(Method->getLocation(), DiagID);

    SourceLocation target = 
        Lexer::GetBeginningOfToken(Method->getBody()->getBeginLoc(),
            ctx.getSourceManager(), ctx.getLangOpts());
    rewriter_.InsertTextBefore(target, "override ");
}

// Обработка случая отсутствия & в range-for
void RefactorHandler::handle_crange_for(const VarDecl* LoopVar, 
    DiagnosticsEngine& diag, ASTContext& ctx)
{
    // Сообщение
    const unsigned diag_id = 
        diag.getCustomDiagID(DiagnosticsEngine::Remark,
            "Найдена постоянная нессылочная переменная в цикле");
    diag.Report(LoopVar->getLocation(), diag_id);

    // Изменение непосредственно кода
    SourceLocation target = 
        Lexer::GetBeginningOfToken(LoopVar->getLocation(), 
            ctx.getSourceManager(), ctx.getLangOpts());
    rewriter_.InsertTextBefore(target, "&");
}

auto NvDtorMatcher()
{
    return cxxRecordDecl(unless(isExpansionInSystemHeader()), // Лексемы из STL исключаются
        unless(isExpansionInFileMatching("boost")), // Лексемы из boost исключаются
        isDerivedFrom(cxxRecordDecl(has(cxxDestructorDecl(unless(isVirtual()), // Деструктор не виртуален
            unless(isImplicit())) // Неявные методы не включаются 
        .bind("nonVirtualDtor")))));
}

auto NoOverrideMatcher()
{
    return cxxMethodDecl(unless(isExpansionInSystemHeader()), // Лексемы из STL исключаются
        unless(isExpansionInFileMatching("boost")), // Лексемы из boost исключаются
        unless(isImplicit()), // исключаются неявные, созданные компилятором методы
        isOverride(), // наследуемый метод...
        unless(hasAttr(attr::Override))) // ...у которого нет override
        .bind("missingOverride");
}

auto NoRefConstVarInRangeLoopMatcher()
{
    return cxxForRangeStmt(hasLoopVariable(
        varDecl(unless(isExpansionInSystemHeader()), // Лексемы из STL исключаются
            unless(isExpansionInFileMatching("boost")), // Лексемы из boost исключаются
            unless(hasType(hasCanonicalType(builtinType()))), // int, double и т.п. исключаются (в т.ч. в auto)
            hasType(isConstQualified()), // const T включаются
            unless(hasType(references(qualType(isConstQualified()))))) // const T& исключаются
        .bind("nonRefLoopVarDecl")));
}

// Конструктор принимает Rewriter для изменения кода.
ComplexConsumer::ComplexConsumer(Rewriter& rewriter) : 
    handler_(rewriter)
{
    // Создаем MatchFinder и добавляем матчеры.
    finder_.addMatcher(NvDtorMatcher(), &handler_);
    finder_.addMatcher(NoOverrideMatcher(), &handler_);
    finder_.addMatcher(NoRefConstVarInRangeLoopMatcher(), &handler_);
}

// Метод HandleTranslationUnit вызывается для каждого файла.
void ComplexConsumer::HandleTranslationUnit(ASTContext& Context)
{
    finder_.matchAST(Context);
}

std::unique_ptr<ASTConsumer> CodeRefactorAction::CreateASTConsumer(CompilerInstance& CI, 
    StringRef file) 
{
    RewriterForCodeRefactor.setSourceMgr(CI.getSourceManager(), 
        CI.getLangOpts());
    
    return std::make_unique<ComplexConsumer>(RewriterForCodeRefactor);
}

bool CodeRefactorAction::BeginSourceFileAction(CompilerInstance& CI)
{
    // Инициализируем Rewriter для рефакторинга.
    RewriterForCodeRefactor.setSourceMgr(CI.getSourceManager(), 
        CI.getLangOpts());
    
    return true;  // Возвращаем true, чтобы продолжить обработку файла.
}

void CodeRefactorAction::EndSourceFileAction()
{
    // Применяем изменения в файле.
    if (RewriterForCodeRefactor.overwriteChangedFiles())
    {
        llvm::errs() << "Error applying changes to files\n";
    }
}