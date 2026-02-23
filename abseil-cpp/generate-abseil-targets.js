#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

/**
 * Parse CMakeLists.txt files to extract Abseil targets
 * Generates abseil-src.h with target definitions
 */

class AbseilTargetParser {
  constructor(repoPath) {
    this.repoPath = repoPath;
    this.targets = [];
  }

  /**
   * Parse a CMakeLists.txt file and extract targets
   */
  parseFile(filePath, basedir) {
    try {
      const content = fs.readFileSync(filePath, 'utf8');
      const lines = content.split('\n');
      
      let i = 0;
      while (i < lines.length) {
        const line = lines[i].trim();
        
        // Match absl_cc_library or absl_cc_test
        if (line.startsWith('absl_cc_library(') || line.startsWith('absl_cc_test(')) {
          const target = this.extractTarget(lines, i, basedir);
          if (target) {
            this.targets.push(target);
            i = target.__endLine;
          } else {
            i++;
          }
        } else {
          i++;
        }
      }
    } catch (error) {
      console.error(`Error parsing ${filePath}:`, error.message);
    }
  }

  /**
   * Extract target info starting from opening parenthesis
   */
  extractTarget(lines, startLine, basedir) {
    const target = {
      basedir: basedir,
      name: '',
      hdrs: [],
      srcs: [],
      copts: [],
      deps: [],
      linkopts: [],
      testonly: false,
      public: false,
      __endLine: startLine + 1
    };

    let parenCount = 1;
    let lineIdx = startLine;
    let currentKey = null;

    // Skip the opening line
    let line = lines[lineIdx];
    parenCount -= (line.match(/\(/g) || []).length - 1;
    lineIdx++;

    while (lineIdx < lines.length && parenCount > 0) {
      line = lines[lineIdx].trim();

      // Count parentheses
      parenCount += (line.match(/\(/g) || []).length;
      parenCount -= (line.match(/\)/g) || []).length;

      if (line && !line.startsWith('#')) {
        // Check for keywords (must be exact match with optional trailing line content)
        if (/^NAME\s*$/.test(line)) {
          currentKey = 'NAME';
        } else if (/^HDRS\s*$/.test(line)) {
          currentKey = 'HDRS';
        } else if (/^SRCS\s*$/.test(line)) {
          currentKey = 'SRCS';
        } else if (/^COPTS\s*$/.test(line)) {
          currentKey = 'COPTS';
        } else if (/^DEPS\s*$/.test(line)) {
          currentKey = 'DEPS';
        } else if (/^LINKOPTS\s*$/.test(line)) {
          currentKey = 'LINKOPTS';
        } else if (/^PUBLIC\s*$/.test(line)) {
          target.public = true;
          currentKey = null;
        } else if (/^TESTONLY\s*$/.test(line)) {
          target.testonly = true;
          currentKey = null;
        } else if (currentKey && line && line !== ')') {
          // Extract value - remove quotes and closing parens
          let value = line.replace(/[")]/g, '').replace(/#.*$/, '').trim();
          if (value && value !== ')') {
            if (currentKey === 'NAME') {
              target.name = value;
            } else if (currentKey === 'HDRS') {
              if (value) target.hdrs.push(value);
            } else if (currentKey === 'SRCS') {
              if (value) target.srcs.push(value);
            } else if (currentKey === 'COPTS') {
              if (value && !value.endsWith('COPTS')) target.copts.push(value);
            } else if (currentKey === 'DEPS') {
              if (value) target.deps.push(value);
            } else if (currentKey === 'LINKOPTS') {
              if (value && !value.endsWith('LINKOPTS')) target.linkopts.push(value);
            }
          }
        }
      }

      lineIdx++;
    }

    target.__endLine = lineIdx;
    return target.name ? target : null;
  }

  /**
   * Recursively find and parse all CMakeLists.txt files
   */
  discoverTargets() {
    const walkDir = (dir) => {
      try {
        const entries = fs.readdirSync(dir, { withFileTypes: true });
        
        for (const entry of entries) {
          const fullPath = path.join(dir, entry.name);
          
          if (entry.isDirectory() && !entry.name.startsWith('.')) {
            walkDir(fullPath);
          } else if (entry.name.toLowerCase() === 'cmakelists.txt') {
            const basedir = path.relative(this.repoPath, dir);
            this.parseFile(fullPath, basedir);
          }
        }
      } catch (error) {
        // Ignore permission errors
      }
    };

    walkDir(path.join(this.repoPath, 'absl'));
  }

  /**
   * Generate C++ header file
   */
  generateHeader() {
    const cppArray = this.targets
      .map(t => {
        const hdrsList = t.hdrs.map(h => `"${h}"`).join(', ');
        const srcsList = t.srcs.map(s => `"${s}"`).join(', ');
        const coptsList = t.copts.map(c => `"${c}"`).join(', ');
        const depsList = t.deps.map(d => `"${d}"`).join(', ');
        const linkoptsList = t.linkopts.map(l => `"${l}"`).join(', ');

        return `    AbseilTarget{
      "${t.basedir}",
      "${t.name}",
      {${hdrsList}},
      {${srcsList}},
      {${coptsList}},
      {${depsList}},
      {${linkoptsList}},
      ${t.testonly},
      ${t.public}
    }`;
      })
      .join(',\n');

    return `#pragma once

#include <vector>
#include <string>

struct AbseilTarget {
  std::string basedir;
  std::string name;
  std::vector<std::string> hdrs;
  std::vector<std::string> srcs;
  std::vector<std::string> copts;
  std::vector<std::string> deps;
  std::vector<std::string> linkopts;
  bool testonly;
  bool public_;
};

inline std::vector<AbseilTarget> get_all_targets() {
  return {
${cppArray}
  };
}
`;
  }

  /**
   * Save header file
   */
  saveHeader(outputPath) {
    const header = this.generateHeader();
    fs.writeFileSync(outputPath, header);
    console.log(`Generated ${outputPath} with ${this.targets.length} targets`);
  }
}

// Main execution
const repoPath = process.argv[2] || path.join(__dirname, '..', '@third_party', 'abseil-cpp', 'repo');
const outputPath = process.argv[3] || path.join(__dirname, 'abseil-src.h');

if (!fs.existsSync(repoPath)) {
  console.error(`Error: Repository path not found: ${repoPath}`);
  process.exit(1);
}

const parser = new AbseilTargetParser(repoPath);
parser.discoverTargets();
parser.saveHeader(outputPath);
