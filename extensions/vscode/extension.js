// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// The C++ executable remains the only XABL compiler and VM.
'use strict';

const path = require('path');
const vscode = require('vscode');

async function launch(checkOnly) {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.uri.scheme !== 'file' ||
        !/\.(?:prg|xabl)$/i.test(editor.document.fileName)) {
        vscode.window.showWarningMessage('Open a local .prg or .xabl file first.');
        return;
    }
    const document = editor.document;
    // Never compile/run an outdated disk version of an edited buffer.
    if (document.isDirty && !(await document.save())) {
        vscode.window.showWarningMessage('Save the XABL source before running a task.');
        return;
    }
    const executable = vscode.workspace.getConfiguration('xabl')
        .get('executablePath', 'xabl');
    if (typeof executable !== 'string' || !executable.trim()) {
        vscode.window.showErrorMessage('Set xabl.executablePath to the XABL CLI.');
        return;
    }

    // ProcessExecution passes filenames as individual arguments, even when
    // they contain spaces. No shell quoting or duplicate language parser.
    const source = document.fileName;
    const folder = vscode.workspace.getWorkspaceFolder(document.uri);
    const task = new vscode.Task(
        { type: 'xabl', operation: checkOnly ? 'check' : 'run' },
        folder || vscode.TaskScope.Global,
        'XABL: ' + (checkOnly ? 'Check ' : 'Run ') + path.basename(source),
        'XABL',
        new vscode.ProcessExecution(
            executable,
            checkOnly ? ['--check', source] : [source],
            { cwd: path.dirname(source) }
        ),
        checkOnly ? ['$gcc'] : []
    );
    await vscode.tasks.executeTask(task);
}

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand('xabl.check', () => launch(true)),
        vscode.commands.registerCommand('xabl.run', () => launch(false))
    );
}

function deactivate() {}
module.exports = { activate, deactivate };
