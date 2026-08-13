const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const {
  buildCommand,
  createEmptyModel,
  analyzeModel,
  parseModel,
  renderAxialForceSvg,
  renderDeformationSvg,
  validateModel,
  serializeModel,
} = require('../web/app.js');

const triangleSource = fs.readFileSync(path.join(__dirname, 'data', 'triangle.model'), 'utf8');
const largeSource = fs.readFileSync(path.join(__dirname, 'data', 'large.model'), 'utf8');
const mediumSource = fs.readFileSync(path.join(__dirname, 'data', 'medium.model'), 'utf8');

const source = `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 1
2 0 -1000

CONSTRAINTS 2
1 1 1
2 0 1
`;

function assertParseFailure(input, label) {
  const parsed = parseModel(input);
  assert.equal(parsed.ok, false, `${label} should fail to parse`);
  assert.equal(typeof parsed.error, 'string');
  assert.ok(parsed.error.length > 0, `${label} should report a parse error`);
}

function assertInvalidModelFromText(input, label) {
  const parsed = parseModel(input);
  assert.equal(parsed.ok, true, `${label} should parse before validation`);
  const validation = validateModel(parsed.model);
  assert.equal(validation.valid, false, `${label} should fail validation`);
  assert.ok(validation.errors.length > 0, `${label} should report validation errors`);
}

function makeValidModel() {
  return {
    nodes: [
      { id: 1, x: 0, y: 0 },
      { id: 2, x: 1000, y: 0 },
    ],
    elements: [
      { id: 1, node1: 1, node2: 2, E: 210000, A: 100 },
    ],
    loads: [
      { node: 2, fx: 0, fy: -1000 },
    ],
    constraints: [
      { node: 1, fix_x: 1, fix_y: 1 },
    ],
  };
}

function cloneModel(model) {
  return JSON.parse(JSON.stringify(model));
}

function renderModelTextUnchecked(model) {
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

function makeMaxCapacityModel() {
  const nodes = Array.from({ length: 10 }, (_, index) => ({
    id: index + 1,
    x: index * 1000,
    y: index % 2 === 0 ? 0 : 500,
  }));
  const elements = [];
  let elementId = 1;
  for (let node1 = 1; node1 <= nodes.length && elements.length < 20; node1 += 1) {
    for (let node2 = node1 + 1; node2 <= nodes.length && elements.length < 20; node2 += 1) {
      elements.push({
        id: elementId,
        node1,
        node2,
        E: 210000 + elementId,
        A: 100 + elementId,
      });
      elementId += 1;
    }
  }
  const loads = Array.from({ length: 10 }, (_, index) => ({
    node: index + 1,
    fx: 0,
    fy: -1000 * (index + 1),
  }));
  const constraints = Array.from({ length: 10 }, (_, index) => ({
    node: index + 1,
    fix_x: index % 2,
    fix_y: (index + 1) % 2,
  }));
  return { nodes, elements, loads, constraints };
}

function makeOverCapacityNodeImportText() {
  const maxModel = makeMaxCapacityModel();
  return renderModelTextUnchecked({
    ...maxModel,
    nodes: [...maxModel.nodes, { id: 11, x: 10000, y: 0 }],
  });
}

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function decodeHtmlAttribute(text) {
  return String(text)
    .replace(/&quot;/g, '"')
    .replace(/&#39;/g, "'")
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&amp;/g, '&');
}

function parseDataAttributes(attributes) {
  const dataset = {};
  const regex = /data-([a-z0-9_-]+)="([^"]*)"/gi;
  for (const match of attributes.matchAll(regex)) {
    dataset[match[1].replace(/-([a-z])/g, (_, letter) => letter.toUpperCase())] =
      decodeHtmlAttribute(match[2]);
  }
  return dataset;
}

function hydrateRenderedRows(host, markup) {
  const rowRegex = /<tr(?: class="([^"]*)")?>([\s\S]*?)<\/tr>/gi;
  for (const rowMatch of markup.matchAll(rowRegex)) {
    const row = makeFakeElement(`row-${host.children.length}`, host.ownerDocument);
    row.tagName = 'TR';
    row.className = rowMatch[1] ?? '';
    host.appendChild(row);

    const rowMarkup = rowMatch[2];
    const controlRegex = /<(input|select|button)\b([^>]*)>([\s\S]*?<\/select>|[\s\S]*?<\/button>|)/gi;
    for (const controlMatch of rowMarkup.matchAll(controlRegex)) {
      const control = makeFakeElement(
        `${controlMatch[1]}-${row.children.length}`,
        host.ownerDocument
      );
      control.tagName = controlMatch[1].toUpperCase();
      control.dataset = parseDataAttributes(controlMatch[2]);
      const classMatch = /class="([^"]*)"/i.exec(controlMatch[2]);
      control.className = classMatch?.[1] ?? '';
      const valueMatch = /value="([^"]*)"/i.exec(controlMatch[2]);
      if (valueMatch) {
        control.value = decodeHtmlAttribute(valueMatch[1]);
      }
      row.appendChild(control);
    }
  }
}

function makeFakeElement(id, ownerDocument = null) {
  const element = {
    id,
    tagName: 'DIV',
    className: '',
    dataset: {},
    disabled: false,
    hidden: false,
    files: [],
    listeners: {},
    children: [],
    parentNode: null,
    clickCount: 0,
    lastWriteMode: null,
    ownerDocument,
    addEventListener(type, listener) {
      this.listeners[type] = listener;
    },
    setAttribute(name, value) {
      this[name] = value;
    },
    removeAttribute(name) {
      delete this[name];
    },
    appendChild(child) {
      this.children.push(child);
      child.parentNode = this;
      return child;
    },
    removeChild(child) {
      this.children = this.children.filter((entry) => entry !== child);
      child.parentNode = null;
      return child;
    },
    contains(node) {
      if (!node) {
        return false;
      }
      if (node === this) {
        return true;
      }
      return this.children.some((child) => child.contains(node));
    },
    focus() {
      if (this.ownerDocument) {
        this.ownerDocument.activeElement = this;
      }
    },
    blur() {
      if (this.ownerDocument?.activeElement === this) {
        this.ownerDocument.activeElement = null;
      }
    },
    setSelectionRange(start, end) {
      this.selectionStart = start;
      this.selectionEnd = end;
    },
    click() {
      this.clickCount += 1;
    },
  };

  let innerHTML = '';
  let textContent = '';
  let value = '';
  Object.defineProperties(element, {
    innerHTML: {
      get() {
        return innerHTML;
      },
      set(nextValue) {
        if (this.ownerDocument?.activeElement && this.contains(this.ownerDocument.activeElement)) {
          this.ownerDocument.activeElement = null;
        }
        innerHTML = String(nextValue);
        textContent = '';
        this.children = [];
        if (this.id.endsWith('-rows')) {
          hydrateRenderedRows(this, innerHTML);
        }
        this.lastWriteMode = 'innerHTML';
      },
    },
    textContent: {
      get() {
        return textContent;
      },
      set(nextValue) {
        textContent = String(nextValue);
        innerHTML = escapeHtml(textContent);
        this.lastWriteMode = 'textContent';
      },
    },
    value: {
      get() {
        return value;
      },
      set(nextValue) {
        value = String(nextValue);
      },
    },
  });

  return element;
}

async function dispatchEvent(element, type, overrides = {}) {
  const listener = element.listeners[type];
  assert.equal(typeof listener, 'function', `${element.id} should register a ${type} listener`);
  const event = {
    target: element,
    currentTarget: element,
    preventDefault() {},
    stopPropagation() {},
    ...overrides,
  };
  return await listener(event);
}

function loadAppWithFakeDom() {
  const ids = [
    'new-model-button',
    'load-example-button',
    'import-model-button',
    'export-model-button',
    'analyze-model-button',
    'add-node-button',
    'add-element-button',
    'add-load-button',
    'add-constraint-button',
    'file-name-input',
    'import-model-input',
    'status-message',
    'error-message',
    'serialized-preview',
    'command-preview',
    'analysis-panel',
    'analysis-status',
    'node-results-rows',
    'element-results-rows',
    'reaction-results-rows',
    'summary-results',
    'deformation-svg',
    'axial-force-svg',
    'nodes-count',
    'elements-count',
    'loads-count',
    'constraints-count',
    'nodes',
    'nodes-rows',
    'elements',
    'elements-rows',
    'loads',
    'loads-rows',
    'constraints',
    'constraints-rows',
  ];
  const fakeDocument = {
    activeElement: null,
    body: null,
    getElementById(id) {
      return elements.get(id) ?? null;
    },
    createElement(tagName) {
      const element = makeFakeElement(`${tagName}-${createdAnchors.length + 1}`, fakeDocument);
      element.tagName = String(tagName).toUpperCase();
      if (element.tagName === 'A') {
        createdAnchors.push(element);
      }
      return element;
    },
  };
  const elements = new Map(ids.map((id) => [id, makeFakeElement(id, fakeDocument)]));
  const createdObjectUrls = [];
  const revokedObjectUrls = [];
  const createdAnchors = [];
  fakeDocument.body = makeFakeElement('body', fakeDocument);
  const fakeWindow = {
    URL: {
      createObjectURL(blob) {
        const value = `blob:fake-${createdObjectUrls.length + 1}`;
        createdObjectUrls.push({ value, blob });
        return value;
      },
      revokeObjectURL(value) {
        revokedObjectUrls.push(value);
      },
    },
  };
  const modulePath = require.resolve('../web/app.js');
  const previousDocument = global.document;
  const previousWindow = global.window;
  const previousBlob = global.Blob;
  const FakeBlob = class FakeBlob {
    constructor(parts, options = {}) {
      this.parts = parts;
      this.type = options.type ?? '';
    }
  };
  delete require.cache[modulePath];
  global.document = fakeDocument;
  global.window = fakeWindow;
  global.Blob = FakeBlob;
  const reloadedModule = require('../web/app.js');
  delete require.cache[modulePath];
  if (previousDocument === undefined) {
    delete global.document;
  } else {
    global.document = previousDocument;
  }
  if (previousWindow === undefined) {
    delete global.window;
  } else {
    global.window = previousWindow;
  }
  if (previousBlob === undefined) {
    delete global.Blob;
  } else {
    global.Blob = previousBlob;
  }
  return {
    elements,
    fakeDocument,
    fakeWindow,
    FakeBlob,
    reloadedModule,
    createdObjectUrls,
    revokedObjectUrls,
    createdAnchors,
  };
}

function getRenderedControl(rowsHost, rowIndex, fieldName) {
  const row = rowsHost.children[rowIndex];
  assert.ok(row, `expected rendered row ${rowIndex + 1}`);
  const control = row.children.find((child) => child.dataset?.field === fieldName);
  assert.ok(control, `expected rendered ${fieldName} control in row ${rowIndex + 1}`);
  return control;
}

const parsed = parseModel(source);
assert.equal(parsed.ok, true);
assert.equal(parsed.model.nodes.length, 2);
assert.equal(validateModel(parsed.model).valid, true);
assert.match(serializeModel(parsed.model), /NODES 2/);
assert.match(serializeModel(parsed.model), /CONSTRAINTS 2/);

const triangleParsed = parseModel(triangleSource);
assert.equal(triangleParsed.ok, true, triangleParsed.error);
assert.equal(triangleParsed.model.nodes.length, 3);
assert.equal(triangleParsed.model.elements.length, 3);

const analyzed = analyzeModel(parsed.model);
assert.equal(analyzed.ok, true);
assert.equal(analyzed.results.nodeDisplacements.length, 2);
assert.equal(analyzed.results.elementResults.length, 1);
assert.equal(analyzed.results.reactions.length, 1);
assert.ok(Number.isFinite(analyzed.results.nodeDisplacements[1].uy));
assert.ok(Number.isFinite(analyzed.results.elementResults[0].stress));
assert.ok(Math.abs(analyzed.results.summary.residualY) < 1e-7);

const analyzedTriangle = analyzeModel(triangleParsed.model);
assert.equal(analyzedTriangle.ok, true, analyzedTriangle.error);
assert.equal(analyzedTriangle.results.nodeDisplacements.length, 3);
assert.equal(analyzedTriangle.results.elementResults.length, 3);

const deformationSvg = renderDeformationSvg(triangleParsed.model, analyzedTriangle.results);
assert.match(deformationSvg, /<svg/);
assert.match(deformationSvg, /data-node/);
assert.doesNotMatch(deformationSvg, /(?:NaN|Infinity)/);

const axialForceSvg = renderAxialForceSvg(analyzedTriangle.results);
assert.match(axialForceSvg, /<svg/);
assert.match(axialForceSvg, /data-element/);
assert.doesNotMatch(axialForceSvg, /(?:NaN|Infinity)/);

const emptyDeformationSvg = renderDeformationSvg(
  { nodes: [], elements: [] },
  { nodeDisplacements: [], elementResults: [] }
);
assert.match(emptyDeformationSvg, /svg-empty-state/);
assert.doesNotMatch(emptyDeformationSvg, /(?:NaN|Infinity)/);

const zeroDisplacementSvg = renderDeformationSvg(
  triangleParsed.model,
  {
    ...analyzedTriangle.results,
    nodeDisplacements: triangleParsed.model.nodes.map((node) => ({
      node: node.id,
      ux: 0,
      uy: 0,
      magnitude: 0,
    })),
  }
);
assert.match(zeroDisplacementSvg, /data-node/);
assert.doesNotMatch(zeroDisplacementSvg, /(?:NaN|Infinity)/);

const offsetModel = {
  nodes: [
    { id: 1, x: 1000000, y: 1000000 },
    { id: 2, x: 1000100, y: 1000000 },
    { id: 3, x: 1000000, y: 1000050 },
  ],
  elements: [
    { id: 1, node1: 1, node2: 2, E: 210000, A: 100 },
    { id: 2, node1: 1, node2: 3, E: 210000, A: 100 },
  ],
};
const offsetDeformationSvg = renderDeformationSvg(offsetModel, {
  nodeDisplacements: offsetModel.nodes.map((node) => ({
    node: node.id,
    ux: 0,
    uy: 0,
    magnitude: 0,
  })),
});
assert.match(offsetDeformationSvg, /viewBox="-14 -14 128 128"/);
assert.match(offsetDeformationSvg, /<line class="deformation-original" x1="0" y1="0" x2="100" y2="0"/);
assert.match(
  offsetDeformationSvg,
  /<g class="svg-legend"><line class="deformation-original" x1="4" y1="8" x2="16" y2="8"/
);
assert.match(offsetDeformationSvg, /原始形状/);
assert.match(offsetDeformationSvg, /变形后形状/);
assert.doesNotMatch(offsetDeformationSvg, /(?:NaN|Infinity)/);

const tinySpanModel = {
  nodes: [
    { id: 1, x: 0, y: 0 },
    { id: 2, x: 1e-12, y: 0 },
    { id: 3, x: 0, y: 1e-12 },
  ],
  elements: [
    { id: 1, node1: 1, node2: 2, E: 210000, A: 100 },
    { id: 2, node1: 1, node2: 3, E: 210000, A: 100 },
    { id: 3, node1: 2, node2: 3, E: 210000, A: 100 },
  ],
  loads: [],
  constraints: [
    { node: 1, fix_x: 1, fix_y: 1 },
    { node: 2, fix_x: 0, fix_y: 1 },
  ],
};
assert.equal(validateModel(tinySpanModel).valid, true);
const tinySpanSvg = renderDeformationSvg(tinySpanModel, {
  nodeDisplacements: tinySpanModel.nodes.map((node) => ({
    node: node.id,
    ux: 0,
    uy: 0,
    magnitude: 0,
  })),
});
const tinySpanViewBox = tinySpanSvg.match(/viewBox="([^"]+)"/)[1].split(' ').map(Number);
assert.ok(tinySpanViewBox[2] > 0 && Number.isFinite(tinySpanViewBox[2]));
assert.ok(tinySpanViewBox[3] > 0 && Number.isFinite(tinySpanViewBox[3]));
assert.match(tinySpanSvg, /data-node/);
assert.match(tinySpanSvg, /原始形状/);
assert.doesNotMatch(tinySpanSvg, /(?:NaN|Infinity)/);

const emptyAxialForceSvg = renderAxialForceSvg({ elementResults: [] });
assert.match(emptyAxialForceSvg, /svg-empty-state/);
assert.doesNotMatch(emptyAxialForceSvg, /(?:NaN|Infinity)/);

const escapedSvg = renderAxialForceSvg({
  elementResults: [
    { element: '1\"><script>alert(1)</script>', axialForce: Number.NaN, status: 'tension' },
  ],
});
assert.doesNotMatch(escapedSvg, /<script/);
assert.match(escapedSvg, /&lt;script&gt;/);
assert.doesNotMatch(escapedSvg, /(?:NaN|Infinity)/);

const extremeForceSvg = renderAxialForceSvg({
  elementResults: [{ element: 1, axialForce: Number.MAX_VALUE, status: 'tension' }],
});
assert.doesNotMatch(extremeForceSvg, /(?:NaN|Infinity)/);

const zeroLengthAnalysis = analyzeModel({
  ...makeValidModel(),
  nodes: [
    { id: 1, x: 0, y: 0 },
    { id: 2, x: 0, y: 0 },
  ],
});
assert.equal(zeroLengthAnalysis.ok, false);
assert.equal(typeof zeroLengthAnalysis.error, 'string');
assert.ok(zeroLengthAnalysis.error.length > 0);
assert.equal('results' in zeroLengthAnalysis, false);

const insufficientConstraintAnalysis = analyzeModel({
  ...makeValidModel(),
  constraints: [{ node: 1, fix_x: 1, fix_y: 1 }],
});
assert.equal(insufficientConstraintAnalysis.ok, false);
assert.equal(typeof insufficientConstraintAnalysis.error, 'string');
assert.ok(insufficientConstraintAnalysis.error.length > 0);
assert.equal('results' in insufficientConstraintAnalysis, false);
assert.equal(triangleParsed.model.loads.length, 1);
assert.equal(triangleParsed.model.constraints.length, 3);
const triangleRoundTrip = parseModel(serializeModel(triangleParsed.model));
assert.equal(triangleRoundTrip.ok, true, triangleRoundTrip.error);
assert.deepEqual(triangleRoundTrip.model, triangleParsed.model);
const serializedTriangle = serializeModel(triangleParsed.model);

const commentRichSource = `
# full-line comments and blank lines are ignored
NODES 2

1 0 0
# node 2
2 500 0

ELEMENTS 1
1 1 2 210000 100

# no loads in this sample
LOADS 0

CONSTRAINTS 1
1 1 1
`;
const commentRichParsed = parseModel(commentRichSource);
assert.equal(commentRichParsed.ok, true, commentRichParsed.error);
assert.equal(commentRichParsed.model.nodes.length, 2);
assert.equal(commentRichParsed.model.elements.length, 1);
assert.equal(commentRichParsed.model.loads.length, 0);
assert.equal(commentRichParsed.model.constraints.length, 1);
const commentRoundTrip = parseModel(serializeModel(commentRichParsed.model));
assert.equal(commentRoundTrip.ok, true, commentRoundTrip.error);
assert.deepEqual(commentRoundTrip.model, commentRichParsed.model);
assert.equal(
  serializeModel(commentRichParsed.model),
  `NODES 2
1 0 0
2 500 0

ELEMENTS 1
1 1 2 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`
);

assertParseFailure(
  `NODES 1
1 0 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0
`,
  'missing constraints section'
);

assertParseFailure(
  `NODES two
1 0 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'malformed section count'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
1 1000 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'duplicate node IDs'
);
assert.match(
  validateModel({
    ...makeValidModel(),
    nodes: [
      { id: 1, x: 0, y: 0 },
      { id: 1, x: 1000, y: 0 },
    ],
  }).errors.join('\n'),
  /NODES.*ID/i
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 2
1 1 2 210000 100
1 2 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'duplicate element IDs'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 3 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'unknown element node references'
);
assert.match(
  validateModel({
    ...makeValidModel(),
    elements: [{ id: 1, node1: 1, node2: 999, E: 210000, A: 100 }],
  }).errors.join('\n'),
  /ELEMENTS.*node/i
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 2
2 0 -1000
2 10 -500

CONSTRAINTS 1
1 1 1
`,
  'duplicate load node records'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 0

CONSTRAINTS 2
1 1 1
1 0 1
`,
  'duplicate constraint node records'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 0 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'non-positive E'
);
assert.match(
  validateModel({
    ...makeValidModel(),
    elements: [{ id: 1, node1: 1, node2: 2, E: 0, A: 100 }],
  }).errors.join('\n'),
  /ELEMENTS.*\bE\b/i
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 0

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'non-positive A'
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 2 210000 100

LOADS 0

CONSTRAINTS 1
1 1 2
`,
  'invalid constraint values'
);
assert.match(
  validateModel({
    ...makeValidModel(),
    constraints: [{ node: 1, fix_x: 2, fix_y: 1 }],
  }).errors.join('\n'),
  /CONSTRAINTS.*fix_[xy]/i
);

assertInvalidModelFromText(
  `NODES 2
1 0 0
2 1000 0

ELEMENTS 1
1 1 1 210000 100

LOADS 0

CONSTRAINTS 1
1 1 1
`,
  'zero-length elements'
);

const tooManyNodes = parseModel(largeSource);
assert.equal(tooManyNodes.ok, true, tooManyNodes.error);
const tooManyNodesValidation = validateModel({
  ...tooManyNodes.model,
  nodes: [...tooManyNodes.model.nodes, { id: 11, x: 5500, y: 0 }],
});
assert.equal(tooManyNodesValidation.valid, false);
assert.ok(tooManyNodesValidation.errors.length > 0);
assert.match(tooManyNodesValidation.errors.join('\n'), /NODES.*10/i);

const tooManyElementsValidation = validateModel({
  ...tooManyNodes.model,
  elements: [
    ...tooManyNodes.model.elements,
    { id: 21, node1: 1, node2: 10, E: 210000, A: 100 },
  ],
});
assert.equal(tooManyElementsValidation.valid, false);
assert.ok(tooManyElementsValidation.errors.length > 0);
assert.match(tooManyElementsValidation.errors.join('\n'), /ELEMENTS.*20/i);

const mediumParsed = parseModel(mediumSource);
assert.equal(mediumParsed.ok, true, mediumParsed.error);
const tooManyLoadsValidation = validateModel({
  ...mediumParsed.model,
  loads: [
    ...mediumParsed.model.loads,
    { node: 1, fx: 0, fy: -1000 },
    { node: 2, fx: 0, fy: -1000 },
    { node: 3, fx: 0, fy: -1000 },
    { node: 4, fx: 0, fy: -1000 },
    { node: 5, fx: 0, fy: -1000 },
    { node: 6, fx: 0, fy: -1000 },
    { node: 7, fx: 0, fy: -1000 },
    { node: 8, fx: 0, fy: -1000 },
  ],
});
assert.equal(tooManyLoadsValidation.valid, false);
assert.ok(tooManyLoadsValidation.errors.length > 0);
assert.match(tooManyLoadsValidation.errors.join('\n'), /LOADS.*10/i);

const tooManyConstraintsValidation = validateModel({
  ...mediumParsed.model,
  constraints: [
    { node: 1, fix_x: 1, fix_y: 1 },
    { node: 2, fix_x: 1, fix_y: 1 },
    { node: 3, fix_x: 1, fix_y: 1 },
    { node: 4, fix_x: 1, fix_y: 1 },
    { node: 5, fix_x: 1, fix_y: 1 },
    { node: 6, fix_x: 1, fix_y: 1 },
    { node: 7, fix_x: 1, fix_y: 1 },
    { node: 8, fix_x: 1, fix_y: 1 },
    { node: 9, fix_x: 1, fix_y: 1 },
    { node: 10, fix_x: 1, fix_y: 1 },
    { node: 11, fix_x: 1, fix_y: 1 },
  ],
});
assert.equal(tooManyConstraintsValidation.valid, false);
assert.ok(tooManyConstraintsValidation.errors.length > 0);
assert.match(tooManyConstraintsValidation.errors.join('\n'), /CONSTRAINTS.*10/i);

for (const missingField of ['nodes', 'elements', 'loads', 'constraints']) {
  const incompleteModel = makeValidModel();
  delete incompleteModel[missingField];

  const validation = validateModel(incompleteModel);
  assert.equal(validation.valid, false, `missing ${missingField} should fail validation`);
  assert.ok(validation.errors.length > 0, `missing ${missingField} should report errors`);
  assert.throws(
    () => serializeModel(incompleteModel),
    (error) =>
      error instanceof Error &&
      !(error instanceof TypeError) &&
      error.message.length > 0,
    `missing ${missingField} should throw a readable serialization error`
  );
}

function runPowerShellQuotingProbe(fileName) {
  const command = buildCommand(fileName);
  const script = `
    Remove-Item Alias:echo -ErrorAction SilentlyContinue
    $script:ExecutedDollar = $false
    function echo {
      param([Parameter(ValueFromRemainingArguments = $true)][object[]]$Args)
      $script:ExecutedDollar = $true
      return 'pwned'
    }
    function fem {
      param([Parameter(ValueFromRemainingArguments = $true)][object[]]$Args)
      $script:CapturedArgs = @($Args)
    }
    Invoke-Expression $env:BUILD_COMMAND
    [pscustomobject]@{
      Args = $script:CapturedArgs
      ExecutedDollar = $script:ExecutedDollar
    } | ConvertTo-Json -Compress
  `;

  const result = spawnSync('powershell.exe', ['-NoProfile', '-Command', script], {
    encoding: 'utf8',
    env: { ...process.env, BUILD_COMMAND: command },
  });

  assert.equal(result.status, 0, result.stderr);
  return JSON.parse(result.stdout.trim());
}

if (process.platform === 'win32') {
  const quotedSpace = runPowerShellQuotingProbe('my model.model');
  assert.deepEqual(quotedSpace.Args, ['--input', 'my model.model']);
  assert.equal(quotedSpace.ExecutedDollar, false);

  const quotedSpecial = runPowerShellQuotingProbe('unsafe;$(echo pwned).model');
  assert.deepEqual(quotedSpecial.Args, ['--input', 'unsafe;$(echo pwned).model']);
  assert.equal(quotedSpecial.ExecutedDollar, false);

  const quotedSingleQuote = runPowerShellQuotingProbe("owner's.model");
  assert.deepEqual(quotedSingleQuote.Args, ['--input', "owner's.model"]);
  assert.equal(quotedSingleQuote.ExecutedDollar, false);
} else {
  console.log(`PowerShell execution test skipped on ${process.platform}`);
}

assert.equal(buildCommand('my model.model'), "fem --input 'my model.model'");
assert.equal(
  buildCommand('unsafe;$(echo pwned).model'),
  "fem --input 'unsafe;$(echo pwned).model'"
);
assert.equal(buildCommand("owner's.model"), "fem --input 'owner''s.model'");

const indexPath = path.join(__dirname, '..', 'web', 'index.html');
assert.equal(fs.existsSync(indexPath), true, 'index.html should exist');
const indexHtml = fs.readFileSync(indexPath, 'utf8');
assert.match(indexHtml, /<link[^>]+href=["']\.\/styles\.css["']/);
assert.match(indexHtml, /<script[^>]+src=["']\.\/app\.js["']/);
assert.match(indexHtml, /id=["']file-name-input["']/);
assert.match(
  indexHtml,
  /id=["']error-message["'][^>]*class=["'][^"']*status--hidden[^"']*["']/,
  'error panel should be hidden in the static HTML before JavaScript initializes'
);
for (const sectionId of ['nodes', 'elements', 'loads', 'constraints']) {
  assert.match(
    indexHtml,
    new RegExp(`id=["']${sectionId}["']`),
    `${sectionId} section should exist in index.html`
  );
}
for (const targetId of [
  'analyze-model-button',
  'analysis-panel',
  'analysis-status',
  'node-results-rows',
  'element-results-rows',
  'reaction-results-rows',
  'summary-results',
  'deformation-svg',
  'axial-force-svg',
]) {
  assert.match(
    indexHtml,
    new RegExp(`id=["']${targetId}["']`),
    `${targetId} should exist in index.html`
  );
}
assert.match(
  indexHtml,
  /id=["']analysis-panel["'][^>]*class=["'][^"']*analysis-panel--hidden[^"']*["']/,
  'analysis panel should be hidden in the static HTML before JavaScript initializes'
);

const browserRuntime = loadAppWithFakeDom();
assert.equal(browserRuntime.fakeDocument.body.dataset.webModelEditorReady, 'true');
assert.deepEqual(browserRuntime.fakeWindow.webModelEditor.state.model, createEmptyModel());
assert.equal(browserRuntime.fakeWindow.webModelEditor.state.analysis, null);
assert.equal(browserRuntime.fakeWindow.webModelEditor.dom.nodes.id, 'nodes');
assert.equal(browserRuntime.elements.get('nodes-count').textContent, '0/10');
assert.equal(browserRuntime.elements.get('elements-count').textContent, '0/20');
assert.equal(browserRuntime.elements.get('loads-count').textContent, '0/10');
assert.equal(browserRuntime.elements.get('constraints-count').textContent, '0/10');
assert.equal(
  browserRuntime.elements.get('status-message').textContent.length > 0,
  true,
  'status message should be initialized'
);
assert.match(browserRuntime.elements.get('status-message').textContent, /[\u4e00-\u9fff]/);
assert.equal(
  browserRuntime.elements.get('command-preview').textContent,
  buildCommand('custom.model')
);
assert.equal(browserRuntime.elements.get('export-model-button').disabled, true);
assert.equal(browserRuntime.elements.get('analyze-model-button').disabled, true);
assert.match(browserRuntime.elements.get('analysis-panel').className, /analysis-panel--hidden/);
assert.doesNotMatch(browserRuntime.elements.get('error-message').className, /status--error/);
assert.equal(browserRuntime.elements.get('error-message').textContent, '');
for (const rowsId of ['nodes-rows', 'elements-rows', 'loads-rows', 'constraints-rows']) {
  assert.equal(
    browserRuntime.elements.get(rowsId).innerHTML.length > 0,
    true,
    `${rowsId} should render placeholder rows`
  );
}

async function runBrowserWorkflowAssertions() {
  const runtime = loadAppWithFakeDom();
  const {
    elements,
    fakeDocument,
    fakeWindow,
    FakeBlob,
    createdObjectUrls,
    revokedObjectUrls,
    createdAnchors,
  } = runtime;

  const previousDocument = global.document;
  const previousWindow = global.window;
  const previousBlob = global.Blob;
  global.document = fakeDocument;
  global.window = fakeWindow;
  global.Blob = FakeBlob;

  try {
    await dispatchEvent(elements.get('load-example-button'), 'click');
    assert.deepEqual(fakeWindow.webModelEditor.state.model, triangleParsed.model);
    assert.equal(elements.get('serialized-preview').textContent, serializedTriangle);
    assert.equal(elements.get('export-model-button').disabled, false);
    assert.match(elements.get('nodes-rows').innerHTML, /value="500"/);
    assert.equal(elements.get('nodes-count').textContent, '3/10');
    assert.equal(elements.get('elements-count').textContent, '3/20');
    assert.equal(elements.get('loads-count').textContent, '1/10');
    assert.equal(elements.get('constraints-count').textContent, '3/10');
    assert.match(elements.get('status-message').textContent, /模型已可导出/);
    assert.equal(elements.get('analyze-model-button').disabled, false);

    await dispatchEvent(elements.get('analyze-model-button'), 'click');
    assert.equal(fakeWindow.webModelEditor.state.analysis.ok, true);
    assert.match(elements.get('analysis-panel').className, /^analysis-panel$/);
    assert.match(
      elements.get('analysis-status').textContent,
      /浏览器.*完成|完成.*浏览器/,
      'successful analysis should explicitly identify browser-side execution'
    );
    for (const targetId of [
      'node-results-rows',
      'element-results-rows',
      'reaction-results-rows',
      'summary-results',
      'deformation-svg',
      'axial-force-svg',
    ]) {
      assert.equal(
        elements.get(targetId).textContent.length > 0 || elements.get(targetId).innerHTML.length > 0,
        true,
        `${targetId} should contain analysis results`
      );
    }

    const duplicateNodeIdInput = {
      dataset: { action: 'edit-field', index: '2', field: 'id' },
      value: '2',
    };
    await dispatchEvent(elements.get('nodes'), 'input', { target: duplicateNodeIdInput });
    assert.equal(fakeWindow.webModelEditor.state.analysis, null);
    assert.match(elements.get('analysis-panel').className, /analysis-panel--hidden/);
    assert.equal(elements.get('node-results-rows').innerHTML, '');
    assert.equal(elements.get('element-results-rows').innerHTML, '');
    assert.equal(elements.get('reaction-results-rows').innerHTML, '');
    assert.equal(elements.get('summary-results').innerHTML, '');
    assert.equal(elements.get('deformation-svg').innerHTML, '');
    assert.equal(elements.get('axial-force-svg').innerHTML, '');
    assert.equal(elements.get('export-model-button').disabled, true);
    assert.match(elements.get('status-message').textContent, /模型暂不能导出/);
    assert.match(elements.get('serialized-preview').textContent, /^# 当前模型无效/);
    assert.match(elements.get('error-message').textContent, /NODES.*ID/i);
    assert.match(getRenderedControl(elements.get('nodes-rows'), 2, 'id').parentNode.className, /invalid/i);

    duplicateNodeIdInput.value = '3';
    await dispatchEvent(elements.get('nodes'), 'input', { target: duplicateNodeIdInput });
    assert.equal(elements.get('export-model-button').disabled, false);
    assert.equal(elements.get('analyze-model-button').disabled, false);
    assert.equal(elements.get('error-message').textContent, '');

    await dispatchEvent(elements.get('analyze-model-button'), 'click');
    assert.equal(fakeWindow.webModelEditor.state.analysis.ok, true);
    duplicateNodeIdInput.value = '2';
    await dispatchEvent(elements.get('nodes'), 'input', { target: duplicateNodeIdInput });
    await dispatchEvent(elements.get('analyze-model-button'), 'click');
    assert.equal(fakeWindow.webModelEditor.state.analysis, null);
    assert.match(elements.get('analysis-panel').className, /analysis-panel--hidden/);
    assert.equal(elements.get('node-results-rows').innerHTML, '');
    assert.match(elements.get('error-message').textContent, /NODES.*ID/i);
    duplicateNodeIdInput.value = '3';
    await dispatchEvent(elements.get('nodes'), 'input', { target: duplicateNodeIdInput });
    assert.doesNotMatch(
      getRenderedControl(elements.get('nodes-rows'), 2, 'id').parentNode.className,
      /invalid/i
    );

    const nodesBeforeEdit = elements.get('nodes-rows').innerHTML;
    const elementsBeforeEdit = elements.get('elements-rows').innerHTML;
    const nodeXInput = {
      dataset: { action: 'edit-field', index: '2', field: 'x' },
      value: '',
    };
    for (const value of ['6', '65', '650']) {
      nodeXInput.value = value;
      await dispatchEvent(elements.get('nodes'), 'input', { target: nodeXInput });
    }
    assert.equal(fakeWindow.webModelEditor.state.model.nodes[2].x, 650);
    assert.equal(nodeXInput.value, '650');
    assert.equal(elements.get('nodes-rows').innerHTML, nodesBeforeEdit);
    assert.equal(elements.get('elements-rows').innerHTML, elementsBeforeEdit);
    assert.match(elements.get('serialized-preview').textContent, /3 650 800/);

    const realNodeIdInput = getRenderedControl(elements.get('nodes-rows'), 2, 'id');
    realNodeIdInput.focus();
    realNodeIdInput.value = '2';
    realNodeIdInput.setSelectionRange(1, 1);
    await dispatchEvent(elements.get('nodes'), 'input', { target: realNodeIdInput });
    assert.equal(fakeWindow.webModelEditor.state.model.nodes[2].id, 2);
    assert.equal(
      fakeDocument.activeElement,
      realNodeIdInput,
      'focus should stay on the live input when the row becomes invalid'
    );
    assert.equal(realNodeIdInput.selectionStart, 1);
    assert.equal(realNodeIdInput.selectionEnd, 1);

    realNodeIdInput.value = '3';
    realNodeIdInput.setSelectionRange(1, 1);
    await dispatchEvent(elements.get('nodes'), 'input', { target: realNodeIdInput });
    assert.equal(fakeWindow.webModelEditor.state.model.nodes[2].id, 3);
    assert.equal(
      fakeDocument.activeElement,
      realNodeIdInput,
      'focus should stay on the live input when the row returns to valid'
    );
    assert.equal(realNodeIdInput.selectionStart, 1);
    assert.equal(realNodeIdInput.selectionEnd, 1);
    assert.equal(elements.get('error-message').textContent, '');

    await dispatchEvent(elements.get('nodes'), 'click', {
      target: {
        dataset: { action: 'add-row', section: 'nodes' },
      },
    });
    assert.equal(fakeWindow.webModelEditor.state.model.nodes.length, 4);

    await dispatchEvent(elements.get('nodes'), 'click', {
      target: {
        dataset: { action: 'delete-row', section: 'nodes', index: '3' },
      },
    });
    assert.equal(fakeWindow.webModelEditor.state.model.nodes.length, 3);

    await dispatchEvent(elements.get('new-model-button'), 'click');
    assert.deepEqual(fakeWindow.webModelEditor.state.model, createEmptyModel());
    assert.equal(elements.get('export-model-button').disabled, true);

    await dispatchEvent(elements.get('import-model-button'), 'click');
    assert.equal(elements.get('import-model-input').clickCount, 1);

    elements.get('import-model-input').files = [
      {
        name: 'triangle.model',
        async text() {
          return triangleSource;
        },
      },
    ];
    await dispatchEvent(elements.get('import-model-input'), 'change', {
      target: elements.get('import-model-input'),
    });
    assert.deepEqual(fakeWindow.webModelEditor.state.model, triangleParsed.model);
    assert.equal(elements.get('serialized-preview').textContent, serializedTriangle);
    assert.equal(elements.get('import-model-input').value, '');

    await dispatchEvent(elements.get('analyze-model-button'), 'click');
    assert.equal(fakeWindow.webModelEditor.state.analysis.ok, true);
    assert.match(elements.get('analysis-panel').className, /^analysis-panel$/);
    assert.match(elements.get('analysis-status').textContent, /浏览器/);

    const firstAnalysisDisplacement = fakeWindow.webModelEditor.state.analysis.results.nodeDisplacements[2].uy;
    const loadFyInput = {
      dataset: { action: 'edit-field', index: '0', field: 'fy' },
      value: '-2000',
    };
    await dispatchEvent(elements.get('loads'), 'input', { target: loadFyInput });
    assert.equal(fakeWindow.webModelEditor.state.analysis, null);
    assert.equal(elements.get('node-results-rows').innerHTML, '');

    await dispatchEvent(elements.get('analyze-model-button'), 'click');
    assert.notEqual(
      fakeWindow.webModelEditor.state.analysis.results.nodeDisplacements[2].uy,
      firstAnalysisDisplacement,
      'editing a load should recompute displacement instead of retaining prior results'
    );

    fakeWindow.webModelEditor.state.model.elements[0].A = 0;
    await dispatchEvent(elements.get('analyze-model-button'), 'click');
    assert.equal(fakeWindow.webModelEditor.state.analysis, null);
    assert.match(elements.get('analysis-panel').className, /analysis-panel--hidden/);
    for (const targetId of [
      'node-results-rows',
      'element-results-rows',
      'reaction-results-rows',
      'summary-results',
      'deformation-svg',
      'axial-force-svg',
    ]) {
      assert.equal(elements.get(targetId).innerHTML, '', `${targetId} should clear after analysis failure`);
    }
    assert.match(elements.get('error-message').textContent, /浏览器端分析失败/);

    elements.get('import-model-input').files = [
      {
        name: 'triangle.model',
        async text() {
          return triangleSource;
        },
      },
    ];
    await dispatchEvent(elements.get('import-model-input'), 'change', {
      target: elements.get('import-model-input'),
    });

    const modelBeforeFailedImport = JSON.parse(JSON.stringify(fakeWindow.webModelEditor.state.model));
    elements.get('import-model-input').files = [
      {
        name: 'broken.model',
        async text() {
          return `NODES 1
1 0 0

ELEMENTS 0

LOADS 0

CONSTRAINTS 0
`;
        },
      },
    ];
    await dispatchEvent(elements.get('import-model-input'), 'change', {
      target: elements.get('import-model-input'),
    });
    assert.deepEqual(fakeWindow.webModelEditor.state.model, modelBeforeFailedImport);
    assert.match(elements.get('error-message').textContent, /element/i);
    assert.match(elements.get('error-message').textContent, /导入失败/);
    assert.equal(elements.get('import-model-input').value, '');

    const maxCapacityModel = makeMaxCapacityModel();
    elements.get('import-model-input').files = [
      {
        name: 'max-capacity.model',
        async text() {
          return serializeModel(maxCapacityModel);
        },
      },
    ];
    await dispatchEvent(elements.get('import-model-input'), 'change', {
      target: elements.get('import-model-input'),
    });
    assert.deepEqual(fakeWindow.webModelEditor.state.model, maxCapacityModel);
    assert.equal(elements.get('nodes-count').textContent, '10/10');
    assert.equal(elements.get('elements-count').textContent, '20/20');
    assert.equal(elements.get('loads-count').textContent, '10/10');
    assert.equal(elements.get('constraints-count').textContent, '10/10');
    for (const addButtonId of [
      'add-node-button',
      'add-element-button',
      'add-load-button',
      'add-constraint-button',
    ]) {
      assert.equal(elements.get(addButtonId).disabled, true, `${addButtonId} should be disabled at limit`);
    }
    const exactCapacitySnapshot = cloneModel(fakeWindow.webModelEditor.state.model);
    await dispatchEvent(elements.get('nodes'), 'click', {
      target: {
        dataset: { action: 'add-row', section: 'nodes' },
      },
    });
    await dispatchEvent(elements.get('elements'), 'click', {
      target: {
        dataset: { action: 'add-row', section: 'elements' },
      },
    });
    await dispatchEvent(elements.get('loads'), 'click', {
      target: {
        dataset: { action: 'add-row', section: 'loads' },
      },
    });
    await dispatchEvent(elements.get('constraints'), 'click', {
      target: {
        dataset: { action: 'add-row', section: 'constraints' },
      },
    });
    assert.deepEqual(fakeWindow.webModelEditor.state.model, exactCapacitySnapshot);

    const overCapacityImportText = makeOverCapacityNodeImportText();
    const overCapacityParsed = parseModel(overCapacityImportText);
    assert.equal(overCapacityParsed.ok, true, overCapacityParsed.error);
    elements.get('import-model-input').files = [
      {
        name: 'over-capacity.model',
        async text() {
          return overCapacityImportText;
        },
      },
    ];
    await dispatchEvent(elements.get('import-model-input'), 'change', {
      target: elements.get('import-model-input'),
    });
    assert.deepEqual(fakeWindow.webModelEditor.state.model, overCapacityParsed.model);
    assert.equal(elements.get('export-model-button').disabled, true);
    assert.match(elements.get('error-message').textContent, /NODES.*10/i);
    assert.equal(elements.get('nodes-count').textContent, '11/10');
    assert.throws(
      () => serializeModel(overCapacityParsed.model),
      (error) => error instanceof Error && /NODES.*10/i.test(error.message)
    );
    await dispatchEvent(elements.get('export-model-button'), 'click');
    assert.equal(createdObjectUrls.length, 0);

    elements.get('import-model-input').files = [
      {
        name: 'triangle.model',
        async text() {
          return triangleSource;
        },
      },
    ];
    await dispatchEvent(elements.get('import-model-input'), 'change', {
      target: elements.get('import-model-input'),
    });
    assert.deepEqual(fakeWindow.webModelEditor.state.model, triangleParsed.model);

    elements.get('file-name-input').value = '<img src=x onerror=1>.model';
    await dispatchEvent(elements.get('file-name-input'), 'input', {
      target: elements.get('file-name-input'),
    });
    assert.equal(
      elements.get('command-preview').textContent,
      buildCommand('<img src=x onerror=1>.model')
    );
    assert.equal(elements.get('command-preview').lastWriteMode, 'textContent');
    assert.doesNotMatch(elements.get('command-preview').innerHTML, /<img/i);

    elements.get('file-name-input').value = '';
    await dispatchEvent(elements.get('file-name-input'), 'input', {
      target: elements.get('file-name-input'),
    });
    await dispatchEvent(elements.get('export-model-button'), 'click');

    assert.equal(createdObjectUrls.length, 1);
    assert.equal(createdObjectUrls[0].blob.type, 'text/plain;charset=utf-8');
    assert.deepEqual(createdObjectUrls[0].blob.parts, [serializedTriangle]);
    assert.equal(createdAnchors.length, 1);
    assert.equal(createdAnchors[0].download, 'custom.model');
    assert.equal(createdAnchors[0].href, 'blob:fake-1');
    assert.equal(createdAnchors[0].clickCount, 1);
    assert.deepEqual(revokedObjectUrls, ['blob:fake-1']);
  } finally {
    if (previousDocument === undefined) {
      delete global.document;
    } else {
      global.document = previousDocument;
    }
    if (previousWindow === undefined) {
      delete global.window;
    } else {
      global.window = previousWindow;
    }
    if (previousBlob === undefined) {
      delete global.Blob;
    } else {
      global.Blob = previousBlob;
    }
  }
}

runBrowserWorkflowAssertions()
  .then(() => {
    console.log('web model tests passed');
  })
  .catch((error) => {
    console.error(error);
    process.exitCode = 1;
  });
