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

function validateUniqueIds(records, label) {
  const seen = new Set();
  for (const record of records) {
    if (!Number.isInteger(record.id) || record.id <= 0 || seen.has(record.id)) {
      return `${label} IDs must be unique positive integers`;
    }
    seen.add(record.id);
  }
  return null;
}

function validateNodeMap(nodes) {
  const nodeIds = new Set();
  for (const node of nodes) {
    if (
      !Number.isInteger(node.id) ||
      node.id <= 0 ||
      !Number.isFinite(node.x) ||
      !Number.isFinite(node.y) ||
      nodeIds.has(node.id)
    ) {
      return 'Nodes must have unique positive integer IDs and finite coordinates';
    }
    nodeIds.add(node.id);
  }
  return nodeIds;
}

function validateRefRecords(records, nodeIds, label, extraCheck) {
  const seen = new Set();
  for (const record of records) {
    const key = record.node;
    if (!Number.isInteger(key) || key <= 0 || seen.has(key) || !nodeIds.has(key)) {
      return `${label} must reference existing nodes exactly once`;
    }
    if (extraCheck && !extraCheck(record)) {
      return `${label} contains invalid values`;
    }
    seen.add(key);
  }
  return null;
}

function validateModel(model) {
  const errors = [];

  if (!model || typeof model !== 'object') {
    return { valid: false, errors: ['Model must be an object'] };
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
    errors.push(`Model must include ${missingArrays.join(', ')} arrays`);
    return { valid: false, errors };
  }

  if (nodes.length < 1) {
    errors.push('Model must contain at least one node');
  }
  if (elements.length < 1) {
    errors.push('Model must contain at least one element');
  }
  if (nodes.length > MAX_NODES) {
    errors.push(`Model cannot contain more than ${MAX_NODES} nodes`);
  }
  if (elements.length > MAX_ELEMENTS) {
    errors.push(`Model cannot contain more than ${MAX_ELEMENTS} elements`);
  }
  if (loads.length > MAX_LOADS) {
    errors.push(`Model cannot contain more than ${MAX_LOADS} loads`);
  }
  if (constraints.length > MAX_CONSTRAINTS) {
    errors.push(`Model cannot contain more than ${MAX_CONSTRAINTS} constraints`);
  }

  const nodeIds = validateNodeMap(nodes);
  if (nodeIds instanceof Set) {
    const elementIdsError = validateUniqueIds(elements, 'Element');
    if (elementIdsError) {
      errors.push(elementIdsError);
    }

    for (const element of elements) {
      if (
        !Number.isInteger(element.id) ||
        element.id <= 0 ||
        !Number.isInteger(element.node1) ||
        !Number.isInteger(element.node2) ||
        !nodeIds.has(element.node1) ||
        !nodeIds.has(element.node2)
      ) {
        errors.push('Elements must reference existing nodes');
        break;
      }
      if (!Number.isFinite(element.E) || !Number.isFinite(element.A) || element.E <= 0 || element.A <= 0) {
        errors.push('Elements must have positive finite E and A');
        break;
      }
      if (element.node1 === element.node2) {
        errors.push('Elements must connect two different nodes');
        break;
      }
    }

    const loadError = validateRefRecords(loads, nodeIds, 'Loads', (record) =>
      Number.isFinite(record.fx) && Number.isFinite(record.fy)
    );
    if (loadError) {
      errors.push(loadError);
    }

    const constraintError = validateRefRecords(
      constraints,
      nodeIds,
      'Constraints',
      (record) =>
        (record.fix_x === 0 || record.fix_x === 1) &&
        (record.fix_y === 0 || record.fix_y === 1)
    );
    if (constraintError) {
      errors.push(constraintError);
    }

    for (const record of constraints) {
      if (
        !Number.isInteger(record.fix_x) ||
        !Number.isInteger(record.fix_y) ||
        (record.fix_x !== 0 && record.fix_x !== 1) ||
        (record.fix_y !== 0 && record.fix_y !== 1)
      ) {
        errors.push('Constraint fix values must be 0 or 1');
        break;
      }
    }
  } else {
    errors.push(nodeIds);
  }

  return { valid: errors.length === 0, errors };
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
    rowsId: 'nodes-rows',
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
    rowsId: 'elements-rows',
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
    rowsId: 'loads-rows',
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
    rowsId: 'constraints-rows',
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

function renderSectionRows(section, model, dom) {
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
      return (
        `<tr>` +
        `${cells}` +
        `<td><button type="button" class="row-action" data-action="delete-row" data-section="${section.key}" data-index="${index}">删除</button></td>` +
        `</tr>`
      );
    })
    .join('');
}

function renderAllSections(dom, model) {
  for (const section of BROWSER_SECTION_CONFIG) {
    renderSectionRows(section, model, dom);
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

  if (validation.valid) {
    const serialized = serializeModel(state.model);
    if (dom.statusMessage) {
      dom.statusMessage.className = 'status status--ok';
      dom.statusMessage.textContent =
        `模型有效：${state.model.nodes.length} 个节点、` +
        `${state.model.elements.length} 个单元、` +
        `${state.model.loads.length} 条荷载、` +
        `${state.model.constraints.length} 条约束。`;
    }
    if (dom.errorMessage) {
      dom.errorMessage.className = 'status status--error';
      dom.errorMessage.textContent = state.lastError || '当前没有错误。';
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
    dom.statusMessage.className = 'status status--error';
    dom.statusMessage.textContent = `模型当前不可导出：${validation.errors.length} 个问题待修正。`;
  }
  if (dom.errorMessage) {
    dom.errorMessage.className = 'status status--error';
    dom.errorMessage.textContent = state.lastError || validation.errors.join('；');
  }
  if (dom.serializedPreview) {
    dom.serializedPreview.textContent =
      '# 当前模型校验失败，无法生成 .model 预览\n' + validation.errors.join('\n');
  }
  if (dom.exportModelButton) {
    dom.exportModelButton.disabled = true;
  }
}

function replaceModel(state, dom, nextModel) {
  state.model = nextModel;
  state.lastError = '';
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

  const validation = validateModel(parsed.model);
  if (!validation.valid) {
    state.lastError = validation.errors.join('；');
    updateStatusPanels(dom, state);
    return false;
  }

  replaceModel(state, dom, parsed.model);
  return true;
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
  renderSectionRows(section, state.model, dom);
  updateStatusPanels(dom, state);
}

function handleSectionClick(section, event, state, dom) {
  const target = event.target;
  if (!target || !target.dataset) {
    return;
  }

  if (target.dataset.action === 'add-row') {
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
