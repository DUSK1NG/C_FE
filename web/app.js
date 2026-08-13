const SECTION_ORDER = ['NODES', 'ELEMENTS', 'LOADS', 'CONSTRAINTS'];
const MAX_NODES = 10;
const MAX_ELEMENTS = 20;
const MAX_LOADS = 10;
const MAX_CONSTRAINTS = 10;

function createEmptyModel() {
  return {
    nodes: [],
    elements: [],
    loads: [],
    constraints: [],
  };
}

function isIgnorableLine(line) {
  const trimmed = line.trim();
  return trimmed === '' || trimmed.startsWith('#');
}

function splitTokens(line) {
  return line.trim().split(/\s+/).filter(Boolean);
}

function parseIntegerToken(token) {
  if (!/^[+-]?\d+$/.test(token)) {
    return null;
  }
  const value = Number(token);
  return Number.isSafeInteger(value) ? value : null;
}

function parseNumberToken(token) {
  if (token === '') {
    return null;
  }
  const value = Number(token);
  return Number.isFinite(value) ? value : null;
}

function readNextContentLine(lines, state) {
  while (state.index < lines.length) {
    const line = lines[state.index++];
    if (!isIgnorableLine(line)) {
      return line.trim();
    }
  }
  return null;
}

function parseHeader(line, expectedName) {
  const tokens = splitTokens(line);
  if (tokens.length !== 2 || tokens[0] !== expectedName) {
    return null;
  }
  const count = parseIntegerToken(tokens[1]);
  if (count === null) {
    return null;
  }
  return count;
}

function parseSection(lines, state, expectedName, fieldCount, recordBuilder, allowZero) {
  const headerLine = readNextContentLine(lines, state);
  if (headerLine === null) {
    throw new Error(`Missing ${expectedName} section`);
  }
  const count = parseHeader(headerLine, expectedName);
  if (count === null || (!allowZero && count <= 0) || count < 0) {
    throw new Error(`Invalid ${expectedName} section header`);
  }

  const records = [];
  for (let i = 0; i < count; i += 1) {
    const line = readNextContentLine(lines, state);
    if (line === null) {
      throw new Error(`Missing ${expectedName} row ${i + 1}`);
    }
    const tokens = splitTokens(line);
    if (tokens.length !== fieldCount) {
      throw new Error(`Invalid ${expectedName} row ${i + 1}`);
    }
    const record = recordBuilder(tokens, i + 1);
    if (record instanceof Error) {
      throw record;
    }
    records.push(record);
  }
  return records;
}

function parseModel(text) {
  if (typeof text !== 'string') {
    return { ok: false, error: 'Model text must be a string' };
  }

  try {
    const lines = text.split(/\r?\n/);
    const state = { index: 0 };
    const model = createEmptyModel();

    model.nodes = parseSection(
      lines,
      state,
      'NODES',
      3,
      (tokens, rowNumber) => {
        const id = parseIntegerToken(tokens[0]);
        const x = parseNumberToken(tokens[1]);
        const y = parseNumberToken(tokens[2]);
        if (id === null || x === null || y === null) {
          return new Error(`Invalid NODES row ${rowNumber}`);
        }
        return { id, x, y };
      },
      false
    );

    model.elements = parseSection(
      lines,
      state,
      'ELEMENTS',
      5,
      (tokens, rowNumber) => {
        const id = parseIntegerToken(tokens[0]);
        const node1 = parseIntegerToken(tokens[1]);
        const node2 = parseIntegerToken(tokens[2]);
        const E = parseNumberToken(tokens[3]);
        const A = parseNumberToken(tokens[4]);
        if (
          id === null ||
          node1 === null ||
          node2 === null ||
          E === null ||
          A === null
        ) {
          return new Error(`Invalid ELEMENTS row ${rowNumber}`);
        }
        return { id, node1, node2, E, A };
      },
      false
    );

    model.loads = parseSection(
      lines,
      state,
      'LOADS',
      3,
      (tokens, rowNumber) => {
        const node = parseIntegerToken(tokens[0]);
        const fx = parseNumberToken(tokens[1]);
        const fy = parseNumberToken(tokens[2]);
        if (node === null || fx === null || fy === null) {
          return new Error(`Invalid LOADS row ${rowNumber}`);
        }
        return { node, fx, fy };
      },
      true
    );

    model.constraints = parseSection(
      lines,
      state,
      'CONSTRAINTS',
      3,
      (tokens, rowNumber) => {
        const node = parseIntegerToken(tokens[0]);
        const fix_x = parseIntegerToken(tokens[1]);
        const fix_y = parseIntegerToken(tokens[2]);
        if (node === null || fix_x === null || fix_y === null) {
          return new Error(`Invalid CONSTRAINTS row ${rowNumber}`);
        }
        return { node, fix_x, fix_y };
      },
      true
    );

    while (state.index < lines.length) {
      if (!isIgnorableLine(lines[state.index])) {
        throw new Error('Unexpected content after CONSTRAINTS section');
      }
      state.index += 1;
    }

    return { ok: true, model };
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : String(error) };
  }
}

const SECTION_LIMITS = {
  nodes: MAX_NODES,
  elements: MAX_ELEMENTS,
  loads: MAX_LOADS,
  constraints: MAX_CONSTRAINTS,
};

const SECTION_LABELS = {
  nodes: 'NODES',
  elements: 'ELEMENTS',
  loads: 'LOADS',
  constraints: 'CONSTRAINTS',
};

function createValidationIssue(sectionKey, rowIndex, field, message) {
  const sectionLabel = SECTION_LABELS[sectionKey] ?? String(sectionKey).toUpperCase();
  const rowText = Number.isInteger(rowIndex) ? ` row ${rowIndex + 1}` : '';
  const fieldText = field ? ` ${field}` : '';
  return {
    sectionKey,
    section: sectionLabel,
    rowIndex,
    field,
    message: `${sectionLabel}${rowText}${fieldText} ${message}`.trim(),
  };
}

function createDuplicateRowIssues(records, sectionKey, idField) {
  const issues = [];
  const seen = new Map();
  const flagged = new Set();

  for (let index = 0; index < records.length; index += 1) {
    const record = records[index];
    const value = record?.id;
    if (!Number.isInteger(value) || value <= 0) {
      issues.push(createValidationIssue(sectionKey, index, idField, 'must be a unique positive integer'));
      continue;
    }
    const previousIndex = seen.get(value);
    if (previousIndex !== undefined) {
      if (!flagged.has(previousIndex)) {
        issues.push(
          createValidationIssue(sectionKey, previousIndex, idField, 'must be a unique positive integer')
        );
        flagged.add(previousIndex);
      }
      issues.push(createValidationIssue(sectionKey, index, idField, 'must be a unique positive integer'));
      flagged.add(index);
      continue;
    }
    seen.set(value, index);
  }

  return issues;
}

function validateNodeMap(nodes) {
  const issues = createDuplicateRowIssues(nodes, 'nodes', 'ID');
  const nodeIds = new Set();

  for (let index = 0; index < nodes.length; index += 1) {
    const node = nodes[index];
    if (Number.isInteger(node?.id) && node.id > 0) {
      nodeIds.add(node.id);
    }
    if (!Number.isFinite(node?.x)) {
      issues.push(createValidationIssue('nodes', index, 'x', 'must be a finite number'));
    }
    if (!Number.isFinite(node?.y)) {
      issues.push(createValidationIssue('nodes', index, 'y', 'must be a finite number'));
    }
  }

  return { nodeIds, issues };
}

function validateElementRecords(elements, nodeIds) {
  const issues = createDuplicateRowIssues(elements, 'elements', 'ID');

  for (let index = 0; index < elements.length; index += 1) {
    const element = elements[index];

    if (!Number.isInteger(element?.node1) || element.node1 <= 0 || !nodeIds.has(element.node1)) {
      issues.push(createValidationIssue('elements', index, 'node1', 'must reference an existing NODES ID'));
    }
    if (!Number.isInteger(element?.node2) || element.node2 <= 0 || !nodeIds.has(element.node2)) {
      issues.push(createValidationIssue('elements', index, 'node2', 'must reference an existing NODES ID'));
    }
    if (
      Number.isInteger(element?.node1) &&
      Number.isInteger(element?.node2) &&
      element.node1 > 0 &&
      element.node2 > 0 &&
      element.node1 === element.node2
    ) {
      issues.push(createValidationIssue('elements', index, 'node2', 'must connect to a different node'));
    }
    if (!Number.isFinite(element?.E) || element.E <= 0) {
      issues.push(createValidationIssue('elements', index, 'E', 'must be a positive finite number'));
    }
    if (!Number.isFinite(element?.A) || element.A <= 0) {
      issues.push(createValidationIssue('elements', index, 'A', 'must be a positive finite number'));
    }
  }

  return issues;
}

function validateNodeReferenceRecords(records, sectionKey, nodeIds, numericFields) {
  const issues = [];
  const seenNodes = new Map();
  const duplicateFlagged = new Set();

  for (let index = 0; index < records.length; index += 1) {
    const record = records[index];
    const nodeId = record?.node;

    if (!Number.isInteger(nodeId) || nodeId <= 0 || !nodeIds.has(nodeId)) {
      issues.push(createValidationIssue(sectionKey, index, 'node', 'must reference an existing NODES ID'));
    } else {
      const previousIndex = seenNodes.get(nodeId);
      if (previousIndex !== undefined) {
        if (!duplicateFlagged.has(previousIndex)) {
          issues.push(
            createValidationIssue(
              sectionKey,
              previousIndex,
              'node',
              `must be unique within ${SECTION_LABELS[sectionKey]}`
            )
          );
          duplicateFlagged.add(previousIndex);
        }
        issues.push(
          createValidationIssue(sectionKey, index, 'node', `must be unique within ${SECTION_LABELS[sectionKey]}`)
        );
        duplicateFlagged.add(index);
      } else {
        seenNodes.set(nodeId, index);
      }
    }

    for (const fieldName of numericFields) {
      const value = record?.[fieldName];
      if (!Number.isFinite(value)) {
        issues.push(createValidationIssue(sectionKey, index, fieldName, 'must be a finite number'));
      }
    }
  }

  return issues;
}

function validateConstraintRecords(constraints, nodeIds) {
  const issues = validateNodeReferenceRecords(constraints, 'constraints', nodeIds, []);

  for (let index = 0; index < constraints.length; index += 1) {
    const constraint = constraints[index];
    if (!Number.isInteger(constraint?.fix_x) || (constraint.fix_x !== 0 && constraint.fix_x !== 1)) {
      issues.push(createValidationIssue('constraints', index, 'fix_x', 'must be 0 or 1'));
    }
    if (!Number.isInteger(constraint?.fix_y) || (constraint.fix_y !== 0 && constraint.fix_y !== 1)) {
      issues.push(createValidationIssue('constraints', index, 'fix_y', 'must be 0 or 1'));
    }
  }

  return issues;
}

function validateModel(model) {
  const issues = [];

  if (!model || typeof model !== 'object') {
    return { valid: false, errors: ['Model must be an object'], details: [] };
  }

  const nodes = Array.isArray(model.nodes) ? model.nodes : null;
  const elements = Array.isArray(model.elements) ? model.elements : null;
  const loads = Array.isArray(model.loads) ? model.loads : null;
  const constraints = Array.isArray(model.constraints) ? model.constraints : null;

  const missingArrays = [];
  if (!nodes) {
    missingArrays.push('nodes');
  }
  if (!elements) {
    missingArrays.push('elements');
  }
  if (!loads) {
    missingArrays.push('loads');
  }
  if (!constraints) {
    missingArrays.push('constraints');
  }

  if (missingArrays.length > 0) {
    return {
      valid: false,
      errors: [`Model must include ${missingArrays.join(', ')} arrays`],
      details: [],
    };
  }

  if (nodes.length < 1) {
    issues.push(createValidationIssue('nodes', null, null, 'requires at least 1 row'));
  }
  if (elements.length < 1) {
    issues.push(createValidationIssue('elements', null, null, 'requires at least 1 row'));
  }

  for (const [sectionKey, limit] of Object.entries(SECTION_LIMITS)) {
    const records = model[sectionKey];
    if (records.length > limit) {
      issues.push(createValidationIssue(sectionKey, null, null, `count ${records.length} exceeds limit ${limit}`));
    }
  }

  const { nodeIds, issues: nodeIssues } = validateNodeMap(nodes);
  issues.push(...nodeIssues);
  issues.push(...validateElementRecords(elements, nodeIds));
  issues.push(...validateNodeReferenceRecords(loads, 'loads', nodeIds, ['fx', 'fy']));
  issues.push(...validateConstraintRecords(constraints, nodeIds));

  return {
    valid: issues.length === 0,
    errors: issues.map((issue) => issue.message),
    details: issues,
  };
}

function serializeModel(model) {
  const validation = validateModel(model);
  if (!validation.valid) {
    throw new Error(validation.errors.join('; '));
  }

  const lines = [];
  lines.push(`NODES ${model.nodes.length}`);
  for (const node of model.nodes) {
    lines.push(`${node.id} ${node.x} ${node.y}`);
  }

  lines.push('');
  lines.push(`ELEMENTS ${model.elements.length}`);
  for (const element of model.elements) {
    lines.push(`${element.id} ${element.node1} ${element.node2} ${element.E} ${element.A}`);
  }

  lines.push('');
  lines.push(`LOADS ${model.loads.length}`);
  for (const load of model.loads) {
    lines.push(`${load.node} ${load.fx} ${load.fy}`);
  }

  lines.push('');
  lines.push(`CONSTRAINTS ${model.constraints.length}`);
  for (const constraint of model.constraints) {
    lines.push(`${constraint.node} ${constraint.fix_x} ${constraint.fix_y}`);
  }

  lines.push('');
  return lines.join('\n');
}

function quotePowerShellArgument(value) {
  const text = String(value);
  return `'${text.split("'").join("''")}'`;
}

function buildCommand(fileName) {
  if (fileName === undefined || fileName === null || fileName === '') {
    return 'fem --input';
  }
  return `fem --input ${quotePowerShellArgument(fileName)}`;
}

const SAMPLE_MODEL_TEXT = `# Reference three-bar truss

NODES 3
1 0 0
2 1000 0
3 500 800

ELEMENTS 3
1 1 2 210000 100
2 1 3 210000 100
3 2 3 210000 100

LOADS 1
3 0 -10000

CONSTRAINTS 3
1 1 1
2 0 1
3 0 0
`;

const BROWSER_SECTION_CONFIG = [
  {
    key: 'nodes',
    title: 'NODES',
    limit: MAX_NODES,
    rowsId: 'nodes-rows',
    countId: 'nodes-count',
    addButtonId: 'add-node-button',
    columnCount: 4,
    placeholder: '暂无节点，点击“新增节点”开始编辑。',
    fields: [
      { key: 'id', inputType: 'number', valueType: 'integer' },
      { key: 'x', inputType: 'number', valueType: 'number', step: 'any' },
      { key: 'y', inputType: 'number', valueType: 'number', step: 'any' },
    ],
    createRow(model) {
      return { id: getNextPositiveId(model.nodes), x: 0, y: 0 };
    },
  },
  {
    key: 'elements',
    title: 'ELEMENTS',
    limit: MAX_ELEMENTS,
    rowsId: 'elements-rows',
    countId: 'elements-count',
    addButtonId: 'add-element-button',
    columnCount: 6,
    placeholder: '暂无单元，点击“新增单元”开始编辑。',
    fields: [
      { key: 'id', inputType: 'number', valueType: 'integer' },
      { key: 'node1', inputType: 'number', valueType: 'integer' },
      { key: 'node2', inputType: 'number', valueType: 'integer' },
      { key: 'E', inputType: 'number', valueType: 'number', step: 'any' },
      { key: 'A', inputType: 'number', valueType: 'number', step: 'any' },
    ],
    createRow(model) {
      const firstNode = model.nodes[0]?.id ?? 1;
      const secondNode = model.nodes[1]?.id ?? firstNode;
      return {
        id: getNextPositiveId(model.elements),
        node1: firstNode,
        node2: secondNode,
        E: 210000,
        A: 100,
      };
    },
  },
  {
    key: 'loads',
    title: 'LOADS',
    limit: MAX_LOADS,
    rowsId: 'loads-rows',
    countId: 'loads-count',
    addButtonId: 'add-load-button',
    columnCount: 4,
    placeholder: '暂无荷载，点击“新增荷载”开始编辑。',
    fields: [
      { key: 'node', inputType: 'number', valueType: 'integer' },
      { key: 'fx', inputType: 'number', valueType: 'number', step: 'any' },
      { key: 'fy', inputType: 'number', valueType: 'number', step: 'any' },
    ],
    createRow(model) {
      return {
        node: model.nodes[model.nodes.length - 1]?.id ?? 1,
        fx: 0,
        fy: -1000,
      };
    },
  },
  {
    key: 'constraints',
    title: 'CONSTRAINTS',
    limit: MAX_CONSTRAINTS,
    rowsId: 'constraints-rows',
    countId: 'constraints-count',
    addButtonId: 'add-constraint-button',
    columnCount: 4,
    placeholder: '暂无约束，点击“新增约束”开始编辑。',
    fields: [
      { key: 'node', inputType: 'number', valueType: 'integer' },
      { key: 'fix_x', inputType: 'select', valueType: 'integer' },
      { key: 'fix_y', inputType: 'select', valueType: 'integer' },
    ],
    createRow(model) {
      return {
        node: model.nodes[0]?.id ?? 1,
        fix_x: 1,
        fix_y: 1,
      };
    },
  },
];

const BROWSER_SECTION_LOOKUP = Object.fromEntries(
  BROWSER_SECTION_CONFIG.map((section) => [
    section.key,
    {
      ...section,
      fieldLookup: Object.fromEntries(section.fields.map((field) => [field.key, field])),
    },
  ])
);

function createBrowserState() {
  return {
    model: createEmptyModel(),
    fileName: 'custom.model',
    lastError: '',
    invalidRowSignature: '',
  };
}

function getBrowserDom(doc) {
  const dom = {
    newModelButton: doc.getElementById('new-model-button'),
    loadExampleButton: doc.getElementById('load-example-button'),
    importModelButton: doc.getElementById('import-model-button'),
    exportModelButton: doc.getElementById('export-model-button'),
    fileNameInput: doc.getElementById('file-name-input'),
    importModelInput: doc.getElementById('import-model-input'),
    statusMessage: doc.getElementById('status-message'),
    errorMessage: doc.getElementById('error-message'),
    serializedPreview: doc.getElementById('serialized-preview'),
    commandPreview: doc.getElementById('command-preview'),
  };

  for (const section of BROWSER_SECTION_CONFIG) {
    dom[section.key] = doc.getElementById(section.key);
    dom[section.rowsId] = doc.getElementById(section.rowsId);
    dom[section.countId] = doc.getElementById(section.countId);
    dom[section.addButtonId] = doc.getElementById(section.addButtonId);
  }

  return dom;
}

function getNextPositiveId(records) {
  let maxId = 0;
  for (const record of records) {
    if (Number.isInteger(record.id) && record.id > maxId) {
      maxId = record.id;
    }
  }
  return maxId + 1;
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function getEffectiveFileName(fileName) {
  const text = String(fileName ?? '').trim();
  return text === '' ? 'custom.model' : text;
}

function parseBrowserValue(rawValue, valueType) {
  const text = String(rawValue ?? '').trim();
  if (valueType === 'integer') {
    const value = parseIntegerToken(text);
    return value === null ? Number.NaN : value;
  }
  const value = parseNumberToken(text);
  return value === null ? Number.NaN : value;
}

function isModelBlank(model) {
  return BROWSER_SECTION_CONFIG.every(
    (section) => Array.isArray(model[section.key]) && model[section.key].length === 0
  );
}

function createInvalidRowMap(validation) {
  const rowMap = {};
  for (const section of BROWSER_SECTION_CONFIG) {
    rowMap[section.key] = new Set();
  }
  for (const issue of validation.details ?? []) {
    if (!Number.isInteger(issue.rowIndex) || !rowMap[issue.sectionKey]) {
      continue;
    }
    rowMap[issue.sectionKey].add(issue.rowIndex);
  }
  return rowMap;
}

function getInvalidRowSignature(rowMap) {
  return BROWSER_SECTION_CONFIG.map((section) => {
    const indices = [...(rowMap[section.key] ?? [])].sort((left, right) => left - right);
    return `${section.key}:${indices.join(',')}`;
  }).join('|');
}

function updateSectionCapacityState(dom, model) {
  for (const section of BROWSER_SECTION_CONFIG) {
    const count = Array.isArray(model[section.key]) ? model[section.key].length : 0;
    if (dom[section.countId]) {
      dom[section.countId].textContent = `${count}/${section.limit}`;
    }
    if (dom[section.addButtonId]) {
      dom[section.addButtonId].disabled = count >= section.limit;
    }
  }
}

function renderFieldControl(field, value, index) {
  if (field.inputType === 'select') {
    const selectedValue = value === 0 || value === 1 ? String(value) : '';
    return (
      `<select data-action="edit-field" data-index="${index}" data-field="${field.key}">` +
      `<option value=""${selectedValue === '' ? ' selected' : ''}>请选择</option>` +
      `<option value="0"${selectedValue === '0' ? ' selected' : ''}>0</option>` +
      `<option value="1"${selectedValue === '1' ? ' selected' : ''}>1</option>` +
      `</select>`
    );
  }

  const stepAttribute = field.step ? ` step="${field.step}"` : '';
  return (
    `<input type="${field.inputType}"${stepAttribute} data-action="edit-field" ` +
    `data-index="${index}" data-field="${field.key}" value="${escapeHtml(value ?? '')}">`
  );
}

function renderSectionRows(section, model, dom, invalidRows = new Set()) {
  const rowsHost = dom[section.rowsId];
  if (!rowsHost) {
    return;
  }

  const records = model[section.key];
  if (!Array.isArray(records) || records.length === 0) {
    rowsHost.innerHTML =
      `<tr><td class="placeholder-cell" colspan="${section.columnCount}">${section.placeholder}</td></tr>`;
    return;
  }

  rowsHost.innerHTML = records
    .map((record, index) => {
      const cells = section.fields
        .map((field) => `<td>${renderFieldControl(field, record[field.key], index)}</td>`)
        .join('');
      const rowClassName = invalidRows.has(index) ? 'editor-row invalid-row' : 'editor-row';
      return (
        `<tr class="${rowClassName}">` +
        `${cells}` +
        `<td><button type="button" class="row-action" data-action="delete-row" data-section="${section.key}" data-index="${index}">删除</button></td>` +
        `</tr>`
      );
    })
    .join('');
}

function renderAllSections(dom, model, invalidRowMap = {}) {
  for (const section of BROWSER_SECTION_CONFIG) {
    renderSectionRows(section, model, dom, invalidRowMap[section.key] ?? new Set());
  }
}

function updateInvalidRowClasses(dom, model, invalidRowMap) {
  for (const section of BROWSER_SECTION_CONFIG) {
    const rowsHost = dom[section.rowsId];
    const records = model[section.key];
    if (!rowsHost || !Array.isArray(records)) {
      continue;
    }

    const invalidRows = invalidRowMap[section.key] ?? new Set();
    for (let index = 0; index < records.length; index += 1) {
      const row = rowsHost.children[index];
      if (row) {
        row.className = invalidRows.has(index) ? 'editor-row invalid-row' : 'editor-row';
      }
    }
  }
}

function updateStatusPanels(dom, state) {
  const validation = validateModel(state.model);
  const effectiveFileName = getEffectiveFileName(state.fileName);

  if (dom.fileNameInput) {
    dom.fileNameInput.value = state.fileName;
  }
  if (dom.commandPreview) {
    dom.commandPreview.textContent = buildCommand(effectiveFileName);
  }

  const invalidRowMap = createInvalidRowMap(validation);
  const invalidRowSignature = getInvalidRowSignature(invalidRowMap);
  const blankModel = isModelBlank(state.model);
  updateSectionCapacityState(dom, state.model);

  if (state.invalidRowSignature !== invalidRowSignature) {
    updateInvalidRowClasses(dom, state.model, invalidRowMap);
    state.invalidRowSignature = invalidRowSignature;
  }

  if (validation.valid) {
    const serialized = serializeModel(state.model);
    if (dom.statusMessage) {
      dom.statusMessage.className = 'status status--ok';
      dom.statusMessage.textContent =
        `模型已可导出：${state.model.nodes.length} 个节点、` +
        `${state.model.elements.length} 个单元、` +
        `${state.model.loads.length} 条荷载、` +
        `${state.model.constraints.length} 条约束。`;
    }
    if (dom.errorMessage) {
      if (state.lastError) {
        dom.errorMessage.className = 'status status--error';
        dom.errorMessage.textContent = state.lastError;
      } else {
        dom.errorMessage.className = 'status status--hidden';
        dom.errorMessage.textContent = '';
      }
    }
    if (dom.serializedPreview) {
      dom.serializedPreview.textContent = serialized;
    }
    if (dom.exportModelButton) {
      dom.exportModelButton.disabled = false;
    }
    return;
  }

  if (dom.statusMessage) {
    dom.statusMessage.className = blankModel && !state.lastError ? 'status status--muted' : 'status status--error';
    dom.statusMessage.textContent =
      blankModel && !state.lastError
        ? '请先添加节点和单元，校验通过后即可导出。'
        : `模型暂不能导出：有 ${validation.errors.length} 个问题需要处理。`;
  }
  if (dom.errorMessage) {
    if (blankModel && !state.lastError) {
      dom.errorMessage.className = 'status status--hidden';
      dom.errorMessage.textContent = '';
    } else {
      dom.errorMessage.className = 'status status--error';
      dom.errorMessage.textContent = state.lastError || validation.errors.join('; ');
    }
  }
  if (dom.serializedPreview) {
    dom.serializedPreview.textContent =
      '# 当前模型无效，不能导出。\n' + validation.errors.join('\n');
  }
  if (dom.exportModelButton) {
    dom.exportModelButton.disabled = true;
  }
}

function replaceModel(state, dom, nextModel) {
  state.model = nextModel;
  state.lastError = '';
  state.invalidRowSignature = '';
  renderAllSections(dom, state.model);
  updateStatusPanels(dom, state);
}

function applyImportText(text, state, dom) {
  const parsed = parseModel(text);
  if (!parsed.ok) {
    state.lastError = parsed.error;
    updateStatusPanels(dom, state);
    return false;
  }

  replaceModel(state, dom, parsed.model);
  return validateModel(parsed.model).valid;
}

function updateSingleField(section, event, state, dom) {
  const target = event.target;
  if (!target || !target.dataset || target.dataset.action !== 'edit-field') {
    return;
  }

  const index = parseIntegerToken(target.dataset.index);
  const fieldName = target.dataset.field;
  if (index === null || index < 0 || !fieldName) {
    return;
  }

  const field = section.fieldLookup[fieldName];
  const records = state.model[section.key];
  if (!field || !Array.isArray(records) || index >= records.length) {
    return;
  }

  records[index] = {
    ...records[index],
    [fieldName]: parseBrowserValue(target.value, field.valueType),
  };
  state.lastError = '';
  updateStatusPanels(dom, state);
}

function handleSectionClick(section, event, state, dom) {
  const target = event.target;
  if (!target || !target.dataset) {
    return;
  }

  if (target.dataset.action === 'add-row') {
    if (state.model[section.key].length >= section.limit) {
      updateStatusPanels(dom, state);
      return;
    }
    state.model[section.key].push(section.createRow(state.model));
    state.lastError = '';
    renderSectionRows(section, state.model, dom);
    updateStatusPanels(dom, state);
    return;
  }

  if (target.dataset.action === 'delete-row') {
    const index = parseIntegerToken(target.dataset.index);
    if (index === null || index < 0 || index >= state.model[section.key].length) {
      return;
    }
    state.model[section.key].splice(index, 1);
    state.lastError = '';
    renderSectionRows(section, state.model, dom);
    updateStatusPanels(dom, state);
  }
}

function downloadModel(state) {
  const validation = validateModel(state.model);
  if (!validation.valid || typeof document === 'undefined') {
    return;
  }

  const serialized = serializeModel(state.model);
  const blob = new Blob([serialized], { type: 'text/plain;charset=utf-8' });
  const urlApi =
    (typeof window !== 'undefined' && window.URL) ||
    (typeof URL !== 'undefined' ? URL : null);
  if (!urlApi || typeof urlApi.createObjectURL !== 'function') {
    return;
  }

  const url = urlApi.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = getEffectiveFileName(state.fileName);
  anchor.hidden = true;
  document.body?.appendChild(anchor);
  anchor.click();
  if (anchor.parentNode) {
    anchor.parentNode.removeChild(anchor);
  }
  if (typeof urlApi.revokeObjectURL === 'function') {
    urlApi.revokeObjectURL(url);
  }
}

function initBrowserApp() {
  if (typeof document === 'undefined') {
    return;
  }

  const state = createBrowserState();
  const dom = getBrowserDom(document);
  const api = {
    createEmptyModel,
    parseModel,
    validateModel,
    serializeModel,
    buildCommand,
    state,
    dom,
  };

  if (dom.newModelButton) {
    dom.newModelButton.addEventListener('click', () => {
      replaceModel(state, dom, createEmptyModel());
    });
  }

  if (dom.loadExampleButton) {
    dom.loadExampleButton.addEventListener('click', () => {
      applyImportText(SAMPLE_MODEL_TEXT, state, dom);
    });
  }

  if (dom.importModelButton && dom.importModelInput) {
    dom.importModelButton.addEventListener('click', () => {
      dom.importModelInput.click();
    });
  }

  if (dom.fileNameInput) {
    const handleFileNameChange = (event) => {
      state.fileName = String(event.target?.value ?? '');
      state.lastError = '';
      updateStatusPanels(dom, state);
    };
    dom.fileNameInput.addEventListener('input', handleFileNameChange);
    dom.fileNameInput.addEventListener('change', handleFileNameChange);
  }

  if (dom.importModelInput) {
    dom.importModelInput.addEventListener('change', async (event) => {
      const input = event.target;
      const file = input?.files?.[0];
      if (!file || typeof file.text !== 'function') {
        return;
      }

      try {
        const text = await file.text();
        applyImportText(text, state, dom);
      } catch (error) {
        state.lastError = error instanceof Error ? error.message : String(error);
        updateStatusPanels(dom, state);
      } finally {
        if (input) {
          input.value = '';
        }
      }
    });
  }

  if (dom.exportModelButton) {
    dom.exportModelButton.addEventListener('click', () => {
      downloadModel(state);
    });
  }

  for (const section of Object.values(BROWSER_SECTION_LOOKUP)) {
    const sectionHost = dom[section.key];
    if (!sectionHost) {
      continue;
    }
    sectionHost.addEventListener('input', (event) => {
      updateSingleField(section, event, state, dom);
    });
    sectionHost.addEventListener('change', (event) => {
      updateSingleField(section, event, state, dom);
    });
    sectionHost.addEventListener('click', (event) => {
      handleSectionClick(section, event, state, dom);
    });
  }

  renderAllSections(dom, state.model);
  updateStatusPanels(dom, state);

  if (typeof window !== 'undefined') {
    window.webModelEditor = api;
  }
  if (document.body) {
    document.body.dataset.webModelEditorReady = 'true';
  }
}

initBrowserApp();

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    createEmptyModel,
    parseModel,
    validateModel,
    serializeModel,
    buildCommand,
  };
}
