/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "qmakemakestep.h"

#include "qmakeparser.h"
#include "qmakeproject.h"
#include "qmakenodes.h"
#include "qmakebuildconfiguration.h"
#include "qmakeprojectmanagerconstants.h"
#include "qmakesettings.h"
#include "qmakestep.h"

#include <coreplugin/variablechooser.h>
#include <projectexplorer/target.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/gnumakeparser.h>
#include <projectexplorer/processparameters.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/xcodebuildparser.h>
#include <utils/qtcprocess.h>

#include <QDir>
#include <QFileInfo>

using ExtensionSystem::PluginManager;
using namespace ProjectExplorer;
using namespace QmakeProjectManager;
using namespace QmakeProjectManager::Internal;

const char MAKESTEP_BS_ID[] = "Qt4ProjectManager.MakeStep";

QmakeMakeStep::QmakeMakeStep(BuildStepList *bsl)
    : MakeStep(bsl, MAKESTEP_BS_ID)
{
    if (bsl->id() == ProjectExplorer::Constants::BUILDSTEPS_CLEAN) {
        setClean(true);
        setUserArguments("clean");
    }
}

bool QmakeMakeStep::init()
{
    const auto bc = static_cast<QmakeBuildConfiguration *>(buildConfiguration());
    if (!bc)
        emit addTask(Task::buildConfigurationMissingTask());

    Utils::FilePath make = effectiveMakeCommand();
    if (make.isEmpty())
        emit addTask(makeCommandMissingTask());

    if (!bc || make.isEmpty()) {
        emitFaultyConfigurationMessage();
        return false;
    }

    // Ignore all but the first make step for a non-top-level build. See QTCREATORBUG-15794.
    m_ignoredNonTopLevelBuild = (bc->fileNodeBuild() || bc->subNodeBuild())
            && static_cast<BuildStepList *>(parent())->firstOfType<QmakeMakeStep>() != this;

    ProcessParameters *pp = processParameters();
    pp->setMacroExpander(bc->macroExpander());

    QString workingDirectory;
    if (bc->subNodeBuild())
        workingDirectory = bc->subNodeBuild()->buildDir();
    else
        workingDirectory = bc->buildDirectory().toString();
    pp->setWorkingDirectory(Utils::FilePath::fromString(workingDirectory));

    pp->setCommand(make);

    // If we are cleaning, then make can fail with a error code, but that doesn't mean
    // we should stop the clean queue
    // That is mostly so that rebuild works on a already clean project
    setIgnoreReturnValue(isClean());

    QString args;

    QmakeProjectManager::QmakeProFileNode *subProFile = bc->subNodeBuild();
    if (subProFile) {
        QString makefile = subProFile->makefile();
        if (makefile.isEmpty())
            makefile = "Makefile";
        // Use Makefile.Debug and Makefile.Release
        // for file builds, since the rules for that are
        // only in those files.
        if (subProFile->isDebugAndRelease() && bc->fileNodeBuild()) {
            if (bc->buildType() == QmakeBuildConfiguration::Debug)
                makefile += ".Debug";
            else
                makefile += ".Release";
        }
        if (makefile != "Makefile") {
            Utils::QtcProcess::addArg(&args, "-f");
            Utils::QtcProcess::addArg(&args, makefile);
        }
        m_makeFileToCheck = QDir(workingDirectory).filePath(makefile);
    } else {
        if (!bc->makefile().isEmpty()) {
            Utils::QtcProcess::addArg(&args, "-f");
            Utils::QtcProcess::addArg(&args, bc->makefile());
            m_makeFileToCheck = QDir(workingDirectory).filePath(bc->makefile());
        } else {
            m_makeFileToCheck = QDir(workingDirectory).filePath("Makefile");
        }
    }

    Utils::QtcProcess::addArgs(&args, allArguments());
    if (bc->fileNodeBuild() && subProFile) {
        QString objectsDir = subProFile->objectsDirectory();
        if (objectsDir.isEmpty()) {
            objectsDir = subProFile->buildDir(bc).toString();
            if (subProFile->isDebugAndRelease()) {
                if (bc->buildType() == QmakeBuildConfiguration::Debug)
                    objectsDir += "/debug";
                else
                    objectsDir += "/release";
            }
        }
        QString relObjectsDir = QDir(pp->workingDirectory().toString())
                .relativeFilePath(objectsDir);
        if (relObjectsDir == ".")
            relObjectsDir.clear();
        if (!relObjectsDir.isEmpty())
            relObjectsDir += '/';
        QString objectFile = relObjectsDir +
                bc->fileNodeBuild()->filePath().toFileInfo().baseName() +
                subProFile->objectExtension();
        Utils::QtcProcess::addArg(&args, objectFile);
    }
    pp->setEnvironment(environment(bc));
    pp->setArguments(args);
    pp->resolveAll();

    setOutputParser(new ProjectExplorer::GnuMakeParser());
    ToolChain *tc = ToolChainKitAspect::toolChain(target()->kit(),
                                                       ProjectExplorer::Constants::CXX_LANGUAGE_ID);
    if (tc && tc->targetAbi().os() == Abi::DarwinOS)
        appendOutputParser(new XcodebuildParser);
    IOutputParser *parser = target()->kit()->createOutputParser();
    if (parser)
        appendOutputParser(parser);
    outputParser()->setWorkingDirectory(pp->effectiveWorkingDirectory());
    appendOutputParser(new QMakeParser); // make may cause qmake to be run, add last to make sure
                                         // it has a low priority.

    m_scriptTarget = (static_cast<QmakeProject *>(bc->target()->project())->rootProjectNode()->projectType() == ProjectType::ScriptTemplate);
    m_unalignedBuildDir = !bc->isBuildDirAtSafeLocation();

    // A user doing "make clean" indicates they want a proper rebuild, so make sure to really
    // execute qmake on the next build.
    if (static_cast<BuildStepList *>(parent())->id()
            == ProjectExplorer::Constants::BUILDSTEPS_CLEAN) {
        const auto qmakeStep = bc->qmakeStep();
        if (qmakeStep)
            qmakeStep->setForced(true);
    }

    return AbstractProcessStep::init();
}

void QmakeMakeStep::doRun()
{
    if (m_scriptTarget || m_ignoredNonTopLevelBuild) {
        emit finished(true);
        return;
    }

    if (!QFileInfo::exists(m_makeFileToCheck)) {
        if (!ignoreReturnValue())
            emit addOutput(tr("Cannot find Makefile. Check your build settings."), BuildStep::OutputFormat::NormalMessage);
        const bool success = ignoreReturnValue();
        emit finished(success);
        return;
    }

    AbstractProcessStep::doRun();
}

void QmakeMakeStep::finish(bool success)
{
    if (!success && !isCanceled() && m_unalignedBuildDir
            && QmakeSettings::warnAgainstUnalignedBuildDir()) {
        const QString msg = tr("The build directory is not at the same level as the source "
                               "directory, which could be the reason for the build failure.");
        emit addTask(Task(Task::Warning, msg, Utils::FilePath(), -1,
                          ProjectExplorer::Constants::TASK_CATEGORY_BUILDSYSTEM));
    }
    MakeStep::finish(success);
}

///
// QmakeMakeStepFactory
///

QmakeMakeStepFactory::QmakeMakeStepFactory()
{
    registerStep<QmakeMakeStep>(MAKESTEP_BS_ID);
    setSupportedProjectType(Constants::QMAKEPROJECT_ID);
    setDisplayName(MakeStep::defaultDisplayName());
}
