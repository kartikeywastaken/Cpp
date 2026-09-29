#include "Parser.h"
#include <iostream>

Parser::Parser(Lexer& lexer, DiagnosticEngine& diag)
    : lexer(lexer), diag(diag) {
    tokens = lexer.tokenizeAll();
}

const Token& Parser::current() const {
    if (cursor < tokens.size()) return tokens[cursor];
    return tokens.back();
}

const Token& Parser::peek(size_t offset) const {
    if (cursor + offset < tokens.size()) return tokens[cursor + offset];
    return tokens.back();
}

Token Parser::advance() {
    if (cursor < tokens.size()) {
        return tokens[cursor++];
    }
    return tokens.back();
}

bool Parser::check(TokenKind kind) const {
    return current().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenKind kind, const std::string& errorMessage) {
    if (check(kind)) {
        return advance();
    }
    diag.error(current().location, errorMessage + " (got '" + current().lexeme + "')");
    return current();
}

void Parser::synchronize() {
    advance();
    while (!check(TokenKind::EndOfFile)) {
        if (current().kind == TokenKind::Semicolon) {
            advance();
            return;
        }
        switch (current().kind) {
            case TokenKind::KW_struct:
            case TokenKind::KW_int:
            case TokenKind::KW_char:
            case TokenKind::KW_void:
            case TokenKind::KW_if:
            case TokenKind::KW_while:
            case TokenKind::KW_for:
            case TokenKind::KW_return:
                return;
            default:
                advance();
                break;
        }
    }
}

bool Parser::isTypeSpecifier(TokenKind kind) const {
    return kind == TokenKind::KW_int || kind == TokenKind::KW_char ||
           kind == TokenKind::KW_void || kind == TokenKind::KW_struct;
}

TypePtr Parser::parseTypeSpecifier() {
    TypePtr type = nullptr;
    if (match(TokenKind::KW_int)) {
        type = Type::getInt();
    } else if (match(TokenKind::KW_char)) {
        type = Type::getChar();
    } else if (match(TokenKind::KW_void)) {
        type = Type::getVoid();
    } else if (match(TokenKind::KW_struct)) {
        Token nameTok = consume(TokenKind::Identifier, "expected struct name");
        type = Type::getStruct(nameTok.lexeme);
    } else {
        diag.error(current().location, "expected type specifier");
        return Type::getInt();
    }
    return type;
}

TypePtr Parser::parseTypeName() {
    TypePtr type = parseTypeSpecifier();
    while (match(TokenKind::Star)) {
        type = Type::getPointer(type);
    }
    return type;
}

TypePtr Parser::parseDeclarator(TypePtr baseType, std::string& outName) {
    while (match(TokenKind::Star)) {
        baseType = Type::getPointer(baseType);
    }
    Token idTok = consume(TokenKind::Identifier, "expected identifier");
    outName = idTok.lexeme;

    // Handle array brackets
    std::vector<size_t> dimensions;
    while (match(TokenKind::LBracket)) {
        if (check(TokenKind::IntLiteral)) {
            int64_t sz = advance().intValue;
            if (sz <= 0) {
                diag.error(current().location, "array size must be positive");
                sz = 1;
            }
            dimensions.push_back(static_cast<size_t>(sz));
            consume(TokenKind::RBracket, "expected ']' after array size");
        } else {
            diag.error(current().location, "expected integer constant for array dimension");
            if (!check(TokenKind::RBracket)) advance();
            consume(TokenKind::RBracket, "expected ']'");
            dimensions.push_back(1);
        }
    }

    // Apply dimensions from right to left (C array type nesting)
    for (auto it = dimensions.rbegin(); it != dimensions.rend(); ++it) {
        baseType = Type::getArray(baseType, *it);
    }

    return baseType;
}

std::shared_ptr<Program> Parser::parseProgram() {
    SourceLocation loc = current().location;
    std::vector<DeclPtr> decls;

    while (!check(TokenKind::EndOfFile)) {
        size_t prevCursor = cursor;
        try {
            auto parsedDecls = parseDeclaration();
            for (auto& d : parsedDecls) {
                if (d) decls.push_back(d);
            }
        } catch (...) {
            synchronize();
        }
        if (cursor == prevCursor) {
            advance();
        }
    }

    return std::make_shared<Program>(loc, decls);
}

std::vector<DeclPtr> Parser::parseDeclaration() {
    if (check(TokenKind::KW_struct) && peek(1).kind == TokenKind::Identifier &&
        (peek(2).kind == TokenKind::LBrace || peek(2).kind == TokenKind::Semicolon)) {
        auto sDecl = parseStructDeclaration();
        if (sDecl) return {sDecl};
        return {};
    }
    return parseFunctionOrVarDeclaration();
}

std::shared_ptr<StructDecl> Parser::parseStructDeclaration() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_struct, "expected 'struct'");
    Token nameTok = consume(TokenKind::Identifier, "expected struct name");

    std::vector<StructMember> members;

    if (match(TokenKind::LBrace)) {
        while (!check(TokenKind::RBrace) && !check(TokenKind::EndOfFile)) {
            TypePtr fieldBase = parseTypeSpecifier();
            while (true) {
                std::string fieldName;
                TypePtr fieldType = parseDeclarator(fieldBase, fieldName);
                members.push_back({fieldName, fieldType, 0});
                if (!match(TokenKind::Comma)) break;
            }
            consume(TokenKind::Semicolon, "expected ';' after struct member declaration");
        }
        consume(TokenKind::RBrace, "expected '}' after struct members");
    }

    consume(TokenKind::Semicolon, "expected ';' after struct declaration");

    TypePtr structType = Type::getStruct(nameTok.lexeme, members);
    return std::make_shared<StructDecl>(loc, nameTok.lexeme, structType);
}

std::vector<DeclPtr> Parser::parseFunctionOrVarDeclaration() {
    SourceLocation loc = current().location;
    if (!isTypeSpecifier(current().kind)) {
        diag.error(loc, "expected declaration specifiers, got '" + current().lexeme + "'");
        synchronize();
        return {};
    }
    TypePtr baseType = parseTypeSpecifier();

    std::string name;
    TypePtr declType = parseDeclarator(baseType, name);

    if (match(TokenKind::LParen)) {
        // Function declaration or definition
        std::vector<std::shared_ptr<ParamDecl>> params;
        if (check(TokenKind::KW_void) && peek(1).kind == TokenKind::RParen) {
            advance(); // consume 'void'
        } else if (!check(TokenKind::RParen)) {
            while (true) {
                params.push_back(parseParam());
                if (!match(TokenKind::Comma)) break;
            }
        }
        consume(TokenKind::RParen, "expected ')' after parameters");

        std::shared_ptr<CompoundStmt> body = nullptr;
        if (check(TokenKind::LBrace)) {
            body = parseCompoundStatement();
        } else {
            consume(TokenKind::Semicolon, "expected ';' or function body");
        }

        return {std::make_shared<FunctionDecl>(loc, name, declType, params, body)};
    } else {
        // Global Variable declaration(s)
        std::vector<DeclPtr> vars;
        while (true) {
            ExprPtr init = nullptr;
            if (match(TokenKind::Equal)) {
                init = parseAssignment();
            }
            vars.push_back(std::make_shared<VarDecl>(loc, name, declType, init, /*isGlobal=*/true));

            if (!match(TokenKind::Comma)) break;

            loc = current().location;
            declType = parseDeclarator(baseType, name);
        }

        consume(TokenKind::Semicolon, "expected ';' after variable declaration");
        return vars;
    }
}

std::shared_ptr<ParamDecl> Parser::parseParam() {
    SourceLocation loc = current().location;
    TypePtr baseType = parseTypeSpecifier();
    std::string name;
    TypePtr paramType = parseDeclarator(baseType, name);

    // In C, array parameter decays to pointer: T[] -> T*
    if (paramType->isArray()) {
        auto arrType = std::static_pointer_cast<ArrayType>(paramType);
        paramType = Type::getPointer(arrType->getElementType());
    }

    return std::make_shared<ParamDecl>(loc, name, paramType);
}

StmtPtr Parser::parseStatement() {
    switch (current().kind) {
        case TokenKind::LBrace:
            return parseCompoundStatement();
        case TokenKind::KW_if:
            return parseIfStatement();
        case TokenKind::KW_while:
            return parseWhileStatement();
        case TokenKind::KW_do:
            return parseDoWhileStatement();
        case TokenKind::KW_for:
            return parseForStatement();
        case TokenKind::KW_return:
            return parseReturnStatement();
        case TokenKind::KW_break:
            return parseBreakStatement();
        case TokenKind::KW_continue:
            return parseContinueStatement();
        default:
            return parseExprOrDeclStatement();
    }
}

StmtPtr Parser::parseDoWhileStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_do, "expected 'do'");
    StmtPtr body = parseStatement();
    consume(TokenKind::KW_while, "expected 'while' after 'do' body");
    consume(TokenKind::LParen, "expected '(' after 'while'");
    ExprPtr cond = parseExpression();
    consume(TokenKind::RParen, "expected ')' after condition");
    consume(TokenKind::Semicolon, "expected ';' after do-while");
    return std::make_shared<DoWhileStmt>(loc, body, cond);
}

std::shared_ptr<CompoundStmt> Parser::parseCompoundStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::LBrace, "expected '{'");
    std::vector<StmtPtr> stmts;

    while (!check(TokenKind::RBrace) && !check(TokenKind::EndOfFile)) {
        size_t prevCursor = cursor;
        try {
            stmts.push_back(parseStatement());
        } catch (...) {
            synchronize();
        }
        if (cursor == prevCursor) {
            advance();
        }
    }

    consume(TokenKind::RBrace, "expected '}'");
    return std::make_shared<CompoundStmt>(loc, stmts);
}

StmtPtr Parser::parseIfStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_if, "expected 'if'");
    consume(TokenKind::LParen, "expected '(' after 'if'");
    ExprPtr cond = parseExpression();
    consume(TokenKind::RParen, "expected ')' after condition");

    StmtPtr thenBranch = parseStatement();
    StmtPtr elseBranch = nullptr;
    if (match(TokenKind::KW_else)) {
        elseBranch = parseStatement();
    }

    return std::make_shared<IfStmt>(loc, cond, thenBranch, elseBranch);
}

StmtPtr Parser::parseWhileStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_while, "expected 'while'");
    consume(TokenKind::LParen, "expected '(' after 'while'");
    ExprPtr cond = parseExpression();
    consume(TokenKind::RParen, "expected ')' after condition");

    StmtPtr body = parseStatement();
    return std::make_shared<WhileStmt>(loc, cond, body);
}

StmtPtr Parser::parseForStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_for, "expected 'for'");
    consume(TokenKind::LParen, "expected '(' after 'for'");

    StmtPtr init = nullptr;
    if (!check(TokenKind::Semicolon)) {
        if (isTypeSpecifier(current().kind)) {
            init = parseVarDeclStatement();
        } else {
            ExprPtr initExpr = parseExpression();
            consume(TokenKind::Semicolon, "expected ';' after for-init expression");
            init = std::make_shared<ExprStmt>(loc, initExpr);
        }
    } else {
        consume(TokenKind::Semicolon, "expected ';'");
    }

    ExprPtr cond = nullptr;
    if (!check(TokenKind::Semicolon)) {
        cond = parseExpression();
    }
    consume(TokenKind::Semicolon, "expected ';' after for condition");

    ExprPtr step = nullptr;
    if (!check(TokenKind::RParen)) {
        step = parseExpression();
    }
    consume(TokenKind::RParen, "expected ')' after for clauses");

    StmtPtr body = parseStatement();
    return std::make_shared<ForStmt>(loc, init, cond, step, body);
}

StmtPtr Parser::parseReturnStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_return, "expected 'return'");
    ExprPtr val = nullptr;
    if (!check(TokenKind::Semicolon)) {
        val = parseExpression();
    }
    consume(TokenKind::Semicolon, "expected ';' after return value");
    return std::make_shared<ReturnStmt>(loc, val);
}

StmtPtr Parser::parseBreakStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_break, "expected 'break'");
    consume(TokenKind::Semicolon, "expected ';' after 'break'");
    return std::make_shared<BreakStmt>(loc);
}

StmtPtr Parser::parseContinueStatement() {
    SourceLocation loc = current().location;
    consume(TokenKind::KW_continue, "expected 'continue'");
    consume(TokenKind::Semicolon, "expected ';' after 'continue'");
    return std::make_shared<ContinueStmt>(loc);
}

StmtPtr Parser::parseExprOrDeclStatement() {
    if (isTypeSpecifier(current().kind)) {
        return parseVarDeclStatement();
    }

    SourceLocation loc = current().location;
    if (match(TokenKind::Semicolon)) {
        return std::make_shared<ExprStmt>(loc, nullptr);
    }

    ExprPtr expr = parseExpression();
    consume(TokenKind::Semicolon, "expected ';' after expression");
    return std::make_shared<ExprStmt>(loc, expr);
}

std::shared_ptr<DeclStmt> Parser::parseVarDeclStatement() {
    SourceLocation loc = current().location;
    TypePtr baseType = parseTypeSpecifier();

    std::vector<std::shared_ptr<VarDecl>> decls;
    while (true) {
        std::string name;
        TypePtr varType = parseDeclarator(baseType, name);
        ExprPtr init = nullptr;
        if (match(TokenKind::Equal)) {
            init = parseAssignment();
        }
        decls.push_back(std::make_shared<VarDecl>(loc, name, varType, init, /*isGlobal=*/false));
        if (!match(TokenKind::Comma)) break;
    }

    consume(TokenKind::Semicolon, "expected ';' after variable declaration");
    return std::make_shared<DeclStmt>(loc, decls);
}

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    SourceLocation loc = current().location;
    ExprPtr expr = parseConditional();

    if (isAssignmentOp(current().kind)) {
        Token opTok = advance();
        AssignOp op = AssignOp::Assign;
        switch (opTok.kind) {
            case TokenKind::Equal:               op = AssignOp::Assign; break;
            case TokenKind::PlusEqual:           op = AssignOp::AddAssign; break;
            case TokenKind::MinusEqual:          op = AssignOp::SubAssign; break;
            case TokenKind::StarEqual:           op = AssignOp::MulAssign; break;
            case TokenKind::SlashEqual:          op = AssignOp::DivAssign; break;
            case TokenKind::PercentEqual:        op = AssignOp::ModAssign; break;
            case TokenKind::AmpEqual:            op = AssignOp::AndAssign; break;
            case TokenKind::PipeEqual:           op = AssignOp::OrAssign; break;
            case TokenKind::CaretEqual:          op = AssignOp::XorAssign; break;
            case TokenKind::LessLessEqual:       op = AssignOp::ShlAssign; break;
            case TokenKind::GreaterGreaterEqual: op = AssignOp::ShrAssign; break;
            default: break;
        }
        ExprPtr rhs = parseAssignment(); // Right-associative
        return std::make_shared<AssignExpr>(loc, op, expr, rhs);
    }

    return expr;
}

ExprPtr Parser::parseConditional() {
    SourceLocation loc = current().location;
    ExprPtr expr = parseLogicalOr();

    if (match(TokenKind::Question)) {
        ExprPtr trueExpr = parseExpression();
        consume(TokenKind::Colon, "expected ':' in conditional expression");
        ExprPtr falseExpr = parseConditional();
        return std::make_shared<ConditionalExpr>(loc, expr, trueExpr, falseExpr);
    }

    return expr;
}

ExprPtr Parser::parseLogicalOr() {
    SourceLocation loc = current().location;
    ExprPtr left = parseLogicalAnd();

    while (match(TokenKind::PipePipe)) {
        ExprPtr right = parseLogicalAnd();
        left = std::make_shared<BinaryExpr>(loc, BinaryOp::LogicalOr, left, right);
    }
    return left;
}

ExprPtr Parser::parseLogicalAnd() {
    SourceLocation loc = current().location;
    ExprPtr left = parseBitwiseOr();

    while (match(TokenKind::AmpAmp)) {
        ExprPtr right = parseBitwiseOr();
        left = std::make_shared<BinaryExpr>(loc, BinaryOp::LogicalAnd, left, right);
    }
    return left;
}

ExprPtr Parser::parseBitwiseOr() {
    SourceLocation loc = current().location;
    ExprPtr left = parseBitwiseXor();

    while (match(TokenKind::Pipe)) {
        ExprPtr right = parseBitwiseXor();
        left = std::make_shared<BinaryExpr>(loc, BinaryOp::BitOr, left, right);
    }
    return left;
}

ExprPtr Parser::parseBitwiseXor() {
    SourceLocation loc = current().location;
    ExprPtr left = parseBitwiseAnd();

    while (match(TokenKind::Caret)) {
        ExprPtr right = parseBitwiseAnd();
        left = std::make_shared<BinaryExpr>(loc, BinaryOp::BitXor, left, right);
    }
    return left;
}

ExprPtr Parser::parseBitwiseAnd() {
    SourceLocation loc = current().location;
    ExprPtr left = parseEquality();

    while (match(TokenKind::Amp)) {
        ExprPtr right = parseEquality();
        left = std::make_shared<BinaryExpr>(loc, BinaryOp::BitAnd, left, right);
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    SourceLocation loc = current().location;
    ExprPtr left = parseRelational();

    while (check(TokenKind::EqualEqual) || check(TokenKind::ExclaimEqual)) {
        BinaryOp op = (advance().kind == TokenKind::EqualEqual) ? BinaryOp::Equal : BinaryOp::NotEqual;
        ExprPtr right = parseRelational();
        left = std::make_shared<BinaryExpr>(loc, op, left, right);
    }
    return left;
}

ExprPtr Parser::parseRelational() {
    SourceLocation loc = current().location;
    ExprPtr left = parseShift();

    while (check(TokenKind::Less) || check(TokenKind::LessEqual) ||
           check(TokenKind::Greater) || check(TokenKind::GreaterEqual)) {
        Token opTok = advance();
        BinaryOp op = BinaryOp::Less;
        if (opTok.kind == TokenKind::Less) op = BinaryOp::Less;
        else if (opTok.kind == TokenKind::LessEqual) op = BinaryOp::LessEqual;
        else if (opTok.kind == TokenKind::Greater) op = BinaryOp::Greater;
        else if (opTok.kind == TokenKind::GreaterEqual) op = BinaryOp::GreaterEqual;

        ExprPtr right = parseShift();
        left = std::make_shared<BinaryExpr>(loc, op, left, right);
    }
    return left;
}

ExprPtr Parser::parseShift() {
    SourceLocation loc = current().location;
    ExprPtr left = parseAdditive();

    while (check(TokenKind::LessLess) || check(TokenKind::GreaterGreater)) {
        BinaryOp op = (advance().kind == TokenKind::LessLess) ? BinaryOp::ShiftLeft : BinaryOp::ShiftRight;
        ExprPtr right = parseAdditive();
        left = std::make_shared<BinaryExpr>(loc, op, left, right);
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    SourceLocation loc = current().location;
    ExprPtr left = parseMultiplicative();

    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        BinaryOp op = (advance().kind == TokenKind::Plus) ? BinaryOp::Add : BinaryOp::Sub;
        ExprPtr right = parseMultiplicative();
        left = std::make_shared<BinaryExpr>(loc, op, left, right);
    }
    return left;
}

ExprPtr Parser::parseMultiplicative() {
    SourceLocation loc = current().location;
    ExprPtr left = parseUnary();

    while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
        Token opTok = advance();
        BinaryOp op = BinaryOp::Mul;
        if (opTok.kind == TokenKind::Star) op = BinaryOp::Mul;
        else if (opTok.kind == TokenKind::Slash) op = BinaryOp::Div;
        else if (opTok.kind == TokenKind::Percent) op = BinaryOp::Mod;

        ExprPtr right = parseUnary();
        left = std::make_shared<BinaryExpr>(loc, op, left, right);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    SourceLocation loc = current().location;

    if (match(TokenKind::Plus)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::Plus, parseUnary(), true);
    }
    if (match(TokenKind::Minus)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::Minus, parseUnary(), true);
    }
    if (match(TokenKind::Exclaim)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::LogicalNot, parseUnary(), true);
    }
    if (match(TokenKind::Tilde)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::BitNot, parseUnary(), true);
    }
    if (match(TokenKind::Star)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::Deref, parseUnary(), true);
    }
    if (match(TokenKind::Amp)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::AddrOf, parseUnary(), true);
    }
    if (match(TokenKind::PlusPlus)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::PreInc, parseUnary(), true);
    }
    if (match(TokenKind::MinusMinus)) {
        return std::make_shared<UnaryExpr>(loc, UnaryOp::PreDec, parseUnary(), true);
    }
    if (match(TokenKind::KW_sizeof)) {
        if (check(TokenKind::LParen) && isTypeSpecifier(peek(1).kind)) {
            advance(); // consume '('
            TypePtr t = parseTypeName();
            consume(TokenKind::RParen, "expected ')' after sizeof type");
            return std::make_shared<SizeofExpr>(loc, t);
        } else {
            ExprPtr operand = parseUnary();
            return std::make_shared<SizeofExpr>(loc, operand);
        }
    }

    return parseCast();
}

ExprPtr Parser::parseCast() {
    SourceLocation loc = current().location;

    // Check for cast: '(' type_specifier ')'
    if (check(TokenKind::LParen) && isTypeSpecifier(peek(1).kind)) {
        advance(); // consume '('
        TypePtr targetType = parseTypeName();
        consume(TokenKind::RParen, "expected ')' in cast");
        ExprPtr operand = parseCast();
        return std::make_shared<CastExpr>(loc, targetType, operand);
    }

    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    SourceLocation loc = current().location;
    ExprPtr expr = parsePrimary();

    while (true) {
        if (match(TokenKind::LParen)) {
            // Function call
            std::vector<ExprPtr> args;
            if (!check(TokenKind::RParen)) {
                while (true) {
                    args.push_back(parseAssignment());
                    if (!match(TokenKind::Comma)) break;
                }
            }
            consume(TokenKind::RParen, "expected ')' after arguments");

            std::string calleeName;
            if (expr->getKind() == ASTKind::VarExpr) {
                calleeName = std::static_pointer_cast<VarExpr>(expr)->getName();
            }
            expr = std::make_shared<CallExpr>(loc, calleeName, args);
        } else if (match(TokenKind::LBracket)) {
            // Array subscript
            ExprPtr index = parseExpression();
            consume(TokenKind::RBracket, "expected ']' after index");
            expr = std::make_shared<SubscriptExpr>(loc, expr, index);
        } else if (match(TokenKind::Dot)) {
            // Struct member access
            Token memberTok = consume(TokenKind::Identifier, "expected member name after '.'");
            expr = std::make_shared<MemberExpr>(loc, expr, memberTok.lexeme, /*isArrow=*/false);
        } else if (match(TokenKind::Arrow)) {
            // Struct pointer member access
            Token memberTok = consume(TokenKind::Identifier, "expected member name after '->'");
            expr = std::make_shared<MemberExpr>(loc, expr, memberTok.lexeme, /*isArrow=*/true);
        } else if (match(TokenKind::PlusPlus)) {
            expr = std::make_shared<UnaryExpr>(loc, UnaryOp::PostInc, expr, /*isPrefix=*/false);
        } else if (match(TokenKind::MinusMinus)) {
            expr = std::make_shared<UnaryExpr>(loc, UnaryOp::PostDec, expr, /*isPrefix=*/false);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    SourceLocation loc = current().location;

    if (check(TokenKind::IntLiteral)) {
        Token tok = advance();
        return std::make_shared<IntegerLiteralExpr>(loc, tok.intValue);
    }
    if (check(TokenKind::CharLiteral)) {
        Token tok = advance();
        return std::make_shared<CharLiteralExpr>(loc, tok.charValue);
    }
    if (check(TokenKind::StringLiteral)) {
        Token tok = advance();
        return std::make_shared<StringLiteralExpr>(loc, tok.stringValue);
    }
    if (check(TokenKind::Identifier)) {
        Token tok = advance();
        return std::make_shared<VarExpr>(loc, tok.lexeme);
    }
    if (match(TokenKind::LParen)) {
        ExprPtr expr = parseExpression();
        consume(TokenKind::RParen, "expected ')' after expression");
        return expr;
    }

    diag.error(loc, "expected expression, got '" + current().lexeme + "'");
    advance();
    return std::make_shared<IntegerLiteralExpr>(loc, 0);
}
