#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include "modulemanager.h"

bool ModuleManager::add(QString name, llvm::Module* mod)
{
    if (has(name)) return true;

    //////
    modules[name] = mod;

    std::unique_ptr<llvm::Module> tmod(mod);
    mee->addModule(std::move(tmod));
    
    return true;
}

bool ModuleManager::remove(QString name)
{
    llvm::Module* mod = modules.value(name);
    modules.remove(name);

    if (mod != NULL) {
        mee->removeModule(mod);
    }
    
    return true;
}

bool ModuleManager::has(QString name)
{
    return modules.contains(name);
    return true;
}

llvm::Module *ModuleManager::get(QString name)
{
    return modules.value(name);
}

/////////
bool ModuleManager::addEE(QString name, llvm::ExecutionEngine* ee)
{
    if (ees.contains(name)) return true;
    
    this->ees[name] = ee;
    return true;
}

bool ModuleManager::removeEE(QString name)
{
    llvm::ExecutionEngine *ee = ees.value(name);
    ees.remove(name);
    
    return true;
}

bool ModuleManager::hasEE(QString name)
{
    return ees.contains(name);
    return true;
}

llvm::ExecutionEngine *ModuleManager::getEE(QString name)
{
    return ees.value(name);
}
