/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#pragma once

#include "pchtasksmergerinterface.h"
#include "pchtask.h"

namespace ClangBackEnd {

class PchTaskQueueInterface;

class PchTasksMerger final : public PchTasksMergerInterface
{
public:
    PchTasksMerger(PchTaskQueueInterface &pchTaskQueue)
        : m_pchTaskQueue(pchTaskQueue)
    {}

    void mergeTasks(PchTaskSets &&taskSets, Utils::SmallStringVector &&toolChainArguments) override;
    void removePchTasks(const Utils::SmallStringVector &projectPartIds) override;

    static CompilerMacros mergeMacros(const CompilerMacros &firstCompilerMacros,
                                      const CompilerMacros &secondCompilerMacros);
    static bool hasDuplicates(const CompilerMacros &compilerMacros);

    static bool mergePchTasks(PchTask &first, PchTask &second);

    static void mergePchTasks(PchTasks &tasks);

private:
    void addProjectTasksToQueue(PchTaskSets &taskSets);
    void mergeSystemTasks(PchTaskSets &taskSets);

private:
    PchTaskQueueInterface &m_pchTaskQueue;
};

} // namespace ClangBackEnd