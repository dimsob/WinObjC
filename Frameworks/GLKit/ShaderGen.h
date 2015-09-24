//******************************************************************************
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//******************************************************************************

#pragma once

#include "ShaderInfo.h"
#include <set>

class ShaderNode;
typedef std::set<std::string> StrSet;
typedef std::vector<ShaderNode*> ShaderNodes;

class ShaderDef {
    ShaderDef(const ShaderDef&); // no copy
    void operator=(const ShaderDef&);

    std::map<std::string, ShaderNode*> def;
public:
    ShaderDef(const std::map<std::string, ShaderNode*>& def) : def(def) {}
    inline const std::map<std::string, ShaderNode*>& getDef() const { return def; }
};

struct TempInfo {
    inline TempInfo() : type(GLKS_INVALID) {}
    inline TempInfo(GLKShaderVarType type, const std::string& body) : type(type), body(body) {}

    bool dependsOn(const StrSet& set) const;
    
    GLKShaderVarType type;
    std::string body;
};
typedef std::map<std::string, TempInfo> TempMap;

@class GLKShaderPair;

class ShaderContext {
    ShaderLayout            shaderVars;

    ShaderMaterial*         inputMaterial;

    const ShaderDef&        vs;
    const ShaderDef&        ps;

    bool                    vertexStage;
    TempMap                 vsTemps, vsTempVals;
    TempMap                 psTemps, psTempVals;
    
protected:
    std::string orderedTempVals(const TempMap& temps, bool usePrecision);
    
    std::string generate(ShaderLayout& outputs, ShaderLayout& inputs, const ShaderDef& shader,
                         const std::string& desc, ShaderLayout* usedOutputs = nullptr);
    
public:
    ShaderContext(const ShaderDef& vert, const ShaderDef& pixel) :
        inputMaterial(nullptr), vs(vert), ps(pixel), vertexStage(false) {}

    // NOTE: neither of these check for overwriting.
    void addTempFunc(GLKShaderVarType type, const std::string& name, const std::string& body);
    void addTempVal(GLKShaderVarType type, const std::string& name, const std::string& body);

    int getIVar(const std::string& name, int def = 0);

    GLKShaderPair* generate(ShaderMaterial& inputs);
};

// --------------------------------------------------------------------------------

class ShaderNode {
    ShaderNode(const ShaderNode&); // no copy
    void operator=(const ShaderNode&);

protected:
    GLKShaderVarType type;
public:
    inline ShaderNode() : type(GLKS_FLOAT4) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) { return false; }
    inline GLKShaderVarType getType() const { return type; }
};

// Check if an ivar is present and non-zero before generating the rest.
class ShaderIVarCheck : public ShaderNode {
    std::string name;
    ShaderNode* node;

public:
    ShaderIVarCheck(const std::string& name, ShaderNode* node) : name(name), node(node) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

// Use a variable if present.
class ShaderVarRef : public ShaderNode {
    std::string name;
    std::string constantResult;
public:
    ShaderVarRef(const std::string& name, const std::string& constantResult = "") : name(name), constantResult(constantResult) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

// Use the first variable that's present, or a constant if none, or nothing if there's no constant.
class ShaderFallbackRef : public ShaderNode {
    std::string first;
    std::string second;
    std::string constantResult;
public:
    ShaderFallbackRef(const std::string& first, const std::string& second, 
                      const std::string& constantResult = "") :
        first(first), second(second), constantResult(constantResult) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderFallbackNode : public ShaderNode {
    std::vector<ShaderNode*> nodes;
public:
    ShaderFallbackNode(std::vector<ShaderNode*> nodes) : nodes(nodes) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

// Use the position variable, applying the mvp matrix.
struct ShaderPosRef : public ShaderNode {
    inline ShaderPosRef() {}
    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

// Texture lookup node.
class ShaderTexRef : public ShaderNode {
    std::string texVar;
    std::string modeVar;
    ShaderNode* uvRef;
    ShaderNode* nextRef;

protected:
    virtual std::string genTexLookup(std::string texVar, std::string uv, ShaderContext& c, ShaderLayout& v);

public:
    ShaderTexRef(const std::string& tex, const std::string& mode, ShaderNode* uvRef, ShaderNode* nextRef) :
        texVar(tex), modeVar(mode), uvRef(uvRef), nextRef(nextRef) {}
    ShaderTexRef(const std::string& tex, ShaderNode* uvRef) :
        texVar(tex), modeVar(""), uvRef(uvRef), nextRef(nullptr) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

// Cube map lookup node.
class ShaderCubeRef : public ShaderTexRef {
    ShaderNode* reflAlphaNode;

protected:
    virtual std::string genTexLookup(std::string texVar, std::string uv, ShaderContext& c, ShaderLayout& v) override;

public:
    ShaderCubeRef(const std::string& tex, const std::string& mode, ShaderNode* reflAlphaNode, 
                  ShaderNode* uvRef, ShaderNode* nextRef) :
        reflAlphaNode(reflAlphaNode),
        ShaderTexRef(tex, mode, uvRef, nextRef) {}
};

class ShaderSpecularTex : public ShaderNode {
    std::string texVar;
    ShaderNode* uvRef;
    ShaderNode* nextRef;

public:
    ShaderSpecularTex(const std::string& tex, ShaderNode* uvRef, ShaderNode* nextRef) :
        texVar(tex), uvRef(uvRef), nextRef(nextRef) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderAdditiveCombiner : public ShaderNode {
    ShaderNodes subNodes;

public:
    inline ShaderAdditiveCombiner() {}
    inline ShaderAdditiveCombiner(const ShaderNodes& n) : subNodes(n) {}

    inline void addNode(ShaderNode* n) { subNodes.push_back(n); }
    
    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderOp : public ShaderNode {
    ShaderNode* n1;
    ShaderNode* n2;
    std::string op;
    bool isOperator;
    bool needsAll;

public:
    inline ShaderOp(ShaderNode* n1, ShaderNode* n2, const std::string& op, bool isOperator, bool needsAll = false) :
        n1(n1), n2(n2), op(op), isOperator(isOperator), needsAll(needsAll) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

// Used to save stuff into a temp.  Only valuable if reused > 1 time.
class ShaderTempRef : public ShaderNode {
    std::string name;
    ShaderNode* body;

public:
    inline ShaderTempRef(GLKShaderVarType t, const std::string& name, ShaderNode* n) :
        name(name), body(n) { type = t; }

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderAttenuator : public ShaderNode {
    ShaderNode* toLight;
    ShaderNode* atten;
public:
    inline ShaderAttenuator(ShaderNode* toLight, ShaderNode* atten) :
        toLight(toLight), atten(atten) { type = GLKS_FLOAT; }

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderReflNode : public ShaderNode {
    ShaderNode* norm;
    ShaderNode* src;
public:
    inline ShaderReflNode(ShaderNode* norm, ShaderNode* src) : norm(norm), src(src) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderCustom : public ShaderNode {
    std::string before, after;
    ShaderNode* inner;
    bool useInner;
public:
    inline ShaderCustom(const std::string& before, const std::string& after = "", ShaderNode* inner = nullptr, bool useInner = true) :
        before(before), after(after), inner(inner), useInner(useInner) {}
    inline ShaderCustom(GLKShaderVarType t, const std::string& before) :
        before(before), after(""), inner(nullptr), useInner(false) { type = t; }

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderLighter : public ShaderNode {
    ShaderNode* lightDir;
    ShaderNode* normal;
    ShaderNode* color;
    ShaderNode* atten;

public:
    inline ShaderLighter(ShaderNode* lightDir, ShaderNode* normal, ShaderNode* color, ShaderNode* atten) :
        lightDir(lightDir), normal(normal), color(color), atten(atten) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderSpecLighter : public ShaderNode {
    ShaderNode* lightDir;
    ShaderNode* cameraDir;
    ShaderNode* normal;
    ShaderNode* color;
    ShaderNode* atten;

public:
    inline ShaderSpecLighter(ShaderNode* lightDir, ShaderNode* cameraDir, ShaderNode* normal,
                             ShaderNode* color, ShaderNode* atten) :
        lightDir(lightDir), cameraDir(cameraDir), normal(normal), color(color), atten(atten) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderSpotlightAtten : public ShaderNode {
    ShaderNode* lightDir;
    ShaderNode* params;
    ShaderNode* dir;

public:
    inline ShaderSpotlightAtten(ShaderNode* lightDir, ShaderNode* params, ShaderNode* dir) :
        lightDir(lightDir), params(params), dir(dir) { type = GLKS_FLOAT; }

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

// Combine n1 and n2, if blend/n1 is not found, only n2 is used.
class ShaderAffineBlend : public ShaderNode {
    ShaderNode* blendNode;
    ShaderNode* n1;
    ShaderNode* n2;

public:
    inline ShaderAffineBlend(ShaderNode* blendNode, ShaderNode* n1, ShaderNode* n2) :
        blendNode(blendNode), n1(n1), n2(n2) {}

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderLinearFog : public ShaderNode {
    ShaderNode* depthRef;
    ShaderNode* fogParams;

public:
    inline ShaderLinearFog(ShaderNode* depthRef, ShaderNode* fogParams) :
        depthRef(depthRef), fogParams(fogParams) { type = GLKS_FLOAT; }

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};

class ShaderExpFog : public ShaderNode {
    ShaderNode* depthRef;
    ShaderNode* densityRef;
    bool squared;

public:
    inline ShaderExpFog(ShaderNode* depthRef, ShaderNode* densityRef, bool squared) :
        depthRef(depthRef), densityRef(densityRef), squared(squared) { type = GLKS_FLOAT; }

    virtual bool generate(std::string& out, ShaderContext& c, ShaderLayout& v) override;
};
