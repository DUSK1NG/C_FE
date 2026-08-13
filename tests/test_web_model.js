const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const {
  buildCommand,
  createEmptyModel,
  parseModel,
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

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function makeFakeElement(id) {
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
        innerHTML = String(nextValue);
        textContent = '';
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
    'file-name-input',
    'import-model-input',
    'status-message',
    'error-message',
    'serialized-preview',
    'command-preview',
    'nodes',
    'nodes-rows',
    'elements',
    'elements-rows',
    'loads',
    'loads-rows',
    'constraints',
    'constraints-rows',
  ];
  const elements = new Map(ids.map((id) => [id, makeFakeElement(id)]));
  const createdObjectUrls = [];
  const revokedObjectUrls = [];
  const createdAnchors = [];
  const fakeDocument = {
    body: makeFakeElement('body'),
    getElementById(id) {
      return elements.get(id) ?? null;
    },
    createElement(tagName) {
      const element = makeFakeElement(`${tagName}-${createdAnchors.length + 1}`);
      element.tagName = String(tagName).toUpperCase();
      if (element.tagName === 'A') {
        createdAnchors.push(element);
      }
      return element;
    },
  };
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

const tooManyElementsValidation = validateModel({
  ...tooManyNodes.model,
  elements: [
    ...tooManyNodes.model.elements,
    { id: 21, node1: 1, node2: 10, E: 210000, A: 100 },
  ],
});
assert.equal(tooManyElementsValidation.valid, false);
assert.ok(tooManyElementsValidation.errors.length > 0);

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
for (const sectionId of ['nodes', 'elements', 'loads', 'constraints']) {
  assert.match(
    indexHtml,
    new RegExp(`id=["']${sectionId}["']`),
    `${sectionId} section should exist in index.html`
  );
}

const browserRuntime = loadAppWithFakeDom();
assert.equal(browserRuntime.fakeDocument.body.dataset.webModelEditorReady, 'true');
assert.deepEqual(browserRuntime.fakeWindow.webModelEditor.state.model, createEmptyModel());
assert.equal(browserRuntime.fakeWindow.webModelEditor.dom.nodes.id, 'nodes');
assert.equal(
  browserRuntime.elements.get('status-message').textContent.length > 0,
  true,
  'status message should be initialized'
);
assert.equal(
  browserRuntime.elements.get('command-preview').textContent,
  buildCommand('custom.model')
);
assert.equal(browserRuntime.elements.get('export-model-button').disabled, true);
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
