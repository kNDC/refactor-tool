#pragma once

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"

#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Refactoring.h"

#include "clang/Rewrite/Core/Rewriter.h"

#include "llvm/Support/CommandLine.h"

#include <unordered_set>

class RefactorHandler : public clang::ast_matchers::MatchFinder::MatchCallback
{
public:
    explicit RefactorHandler(clang::Rewriter& rewriter) : 
        rewriter_(rewriter)
    {}

    // Метод run вызывается для каждого совпадения с матчем. 
    // Мы проверяем тип совпадения по bind-именам и применяем рефакторинг.
    virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& Result) override;

private:
    // 1. Невиртуальные деструкторы
    void handle_nv_dtor(const clang::CXXDestructorDecl* Dtor, 
        clang::DiagnosticsEngine& Diag, clang::ASTContext& ctx);

    // 2. Методы без override
    void handle_miss_override(const clang::CXXMethodDecl* Method, 
        clang::DiagnosticsEngine& Diag, clang::ASTContext& ctx);

    // 3. range-for без &
    void handle_crange_for(const clang::VarDecl* LoopVar, 
        clang::DiagnosticsEngine& Diag, clang::ASTContext& ctx);
    
private:
    clang::Rewriter& rewriter_;

    // Для хранения позиций деструкторов, к которым уже добавлен virtual
    std::unordered_set<uint64_t> nonvirt_dtors_;
};

class ComplexConsumer : public clang::ASTConsumer
{
private:
    RefactorHandler handler_; // Обработчик матчеров.
    clang::ast_matchers::MatchFinder finder_; // MatchFinder для поиска узлов AST.

public:
    // Конструктор принимает Rewriter для изменения кода.
    explicit ComplexConsumer(clang::Rewriter& rewriter);

    // Метод HandleTranslationUnit вызывается для каждого файла.
    void HandleTranslationUnit(clang::ASTContext& context) override;
};

class CodeRefactorAction : public clang::ASTFrontendAction
{
private:
    clang::Rewriter RewriterForCodeRefactor;

public:
    // Returns our ASTConsumer per translation unit.
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, 
        clang::StringRef file) override;
    virtual bool BeginSourceFileAction(clang::CompilerInstance& CI) override;
    virtual void EndSourceFileAction() override;
};