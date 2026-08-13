const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const {
  buildCommand,
  parseModel,
  validateModel,
  serializeModel,
} = require('../web/app.js');

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

const parsed = parseModel(source);
assert.equal(parsed.ok, true);
assert.equal(parsed.model.nodes.length, 2);
assert.equal(validateModel(parsed.model).valid, true);
assert.match(serializeModel(parsed.model), /NODES 2/);
assert.match(serializeModel(parsed.model), /CONSTRAINTS 2/);

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

console.log('web model tests passed');
