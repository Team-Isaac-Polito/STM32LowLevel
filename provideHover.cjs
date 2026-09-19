/** @typedef { import('vscode') } vscode */
const fs = require('fs');

/**
 * Universal macro hover parser using a generalized comment trend
 * @param {typeof import('vscode')} vscode 
 * @param {vscode.TextDocument} document 
 * @param {vscode.Position} position 
 */
async function provideHover(vscode, document, position) {
    if (!document.languageId.match(/^(c|cpp)$/)) return;

    const wordRange = document.getWordRangeAtPosition(position);
    if (!wordRange) return;
    const hoveredWord = document.getText(wordRange);

    // Match trend 1: #define {word} {value} //[wildcard] {comment until end of line}
    // Match trend 2: #define {word} {value} /*[wildcard] {comment across multi-lines} */
    const macroRegex = new RegExp(`#define\\s+${hoveredWord}\\b.*?\\/(?:\\/\\S*\\s*(.*)|\\*\\S*\\s*([\\s\\S]*?)\\*\\/)`);

    let foundComment = "";

    // Check current file buffer
    const activeMatch = document.getText().match(macroRegex);
    if (activeMatch) {
        // Capture group 1 is populated for single-line comments; group 2 is populated for multi-line block comments
        foundComment = activeMatch[1] || activeMatch[2];
    } else if (vscode.workspace.workspaceFolders) {
        // Fallback: Scan project headers for the same trend
        const files = await vscode.workspace.findFiles('**/*.{h,hpp,inc}', '**/build/**');
        for (const file of files) {
            try {
                const fileMatch = fs.readFileSync(file.fsPath, 'utf8').match(macroRegex);
                if (fileMatch) {
                    foundComment = fileMatch[1] || fileMatch[2];
                    break;
                }
            } catch (e) {}
        }
    }

    // Output the comment cleanly if found
    if (foundComment) {
        // Clean layout alignment spaces and join multi-lines
        const cleanComment = foundComment
            .split(/\r?\n/)
            .map(line => line.trim())
            .filter(line => line.length > 0)
            .join(' ');

        return new vscode.Hover(
            new vscode.MarkdownString(`### Brief\n\n${cleanComment}`)
        );
    }
}

module.exports = { provideHover };
