const state = {
  selectedRun: null,
  scenarios: [],
};

async function api(path, options = {}) {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json' },
    ...options,
  });
  const data = await res.json();
  if (!res.ok) throw new Error(data.error || `HTTP ${res.status}`);
  return data;
}

function renderHealth(data) {
  document.getElementById('health').innerHTML = `
    <div><strong>Project root:</strong> ${data.project_root}</div>
    <div><strong>Capture tool:</strong> ${data.capture_tool || 'not found'}</div>
  `;
}

function createScenarioCard(scenario) {
  const formId = `scenario-${scenario.id}`;
  const fields = scenario.args.map(arg => `
    <label>
      ${arg.label}
      <input name="${arg.name}" value="${arg.default || ''}">
    </label>
  `).join('');
  return `
    <form class="scenario-card" id="${formId}">
      <div class="scenario-head">
        <h3>${scenario.title}</h3>
        <span class="kind">${scenario.kind}</span>
      </div>
      <div class="script">${scenario.script}</div>
      <div class="grid-form">${fields}</div>
      <div class="actions">
        <button type="submit">Run</button>
      </div>
    </form>
  `;
}

function bindScenarioForms() {
  for (const scenario of state.scenarios) {
    const form = document.getElementById(`scenario-${scenario.id}`);
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const fd = new FormData(form);
      const args = Object.fromEntries(fd.entries());
      try {
        await api('/api/run', {
          method: 'POST',
          body: JSON.stringify({ scenario: scenario.id, args }),
        });
        await refreshRuns();
        await refreshFiles();
      } catch (err) {
        alert(err.message);
      }
    });
  }
}

async function refreshScenarios() {
  state.scenarios = await api('/api/scenarios');
  document.getElementById('scenarios').innerHTML = state.scenarios.map(createScenarioCard).join('');
  bindScenarioForms();
}

function renderRuns(runs) {
  const html = runs.map(run => `
    <div class="run-row ${state.selectedRun === run.id ? 'selected' : ''}">
      <div>
        <strong>${run.title}</strong>
        <div class="muted">${run.id}</div>
        <div class="muted">${run.cmd.join(' ')}</div>
      </div>
      <div>
        <div><strong>${run.running ? 'running' : 'finished'}</strong></div>
        <div class="muted">rc=${run.returncode ?? '-'}</div>
      </div>
      <div class="actions inline">
        <button data-run-open="${run.id}">Log</button>
        <button data-run-stop="${run.id}">Stop</button>
      </div>
    </div>
  `).join('');
  document.getElementById('runs').innerHTML = html || '<div class="muted">No runs yet.</div>';

  document.querySelectorAll('[data-run-open]').forEach(btn => {
    btn.addEventListener('click', async () => {
      state.selectedRun = btn.dataset.runOpen;
      await refreshRuns();
      await refreshLog();
    });
  });
  document.querySelectorAll('[data-run-stop]').forEach(btn => {
    btn.addEventListener('click', async () => {
      try {
        await api(`/api/runs/${btn.dataset.runStop}/stop`, { method: 'POST', body: '{}' });
        await refreshRuns();
      } catch (err) {
        alert(err.message);
      }
    });
  });
}

async function refreshRuns() {
  const runs = await api('/api/runs');
  renderRuns(runs);
}

async function refreshLog() {
  if (!state.selectedRun) return;
  const data = await api(`/api/runs/${state.selectedRun}/log`);
  document.getElementById('log-view').textContent = data.log || '(empty log)';
}

async function refreshCapture() {
  const data = await api('/api/capture');
  document.getElementById('capture-status').textContent = JSON.stringify(data, null, 2);
}

async function refreshFiles() {
  const files = await api('/api/files');
  document.getElementById('files').innerHTML = files.map(file => `
    <div class="file-row">
      <span>${file.name}</span>
      <span class="muted">${file.path}</span>
      <span class="muted">${file.size} bytes</span>
    </div>
  `).join('') || '<div class="muted">No files yet.</div>';
}

async function initCaptureForm() {
  document.getElementById('capture-form').addEventListener('submit', async (event) => {
    event.preventDefault();
    const fd = new FormData(event.target);
    const payload = Object.fromEntries(fd.entries());
    try {
      await api('/api/capture/start', { method: 'POST', body: JSON.stringify(payload) });
      await refreshCapture();
      await refreshFiles();
    } catch (err) {
      alert(err.message);
    }
  });

  document.getElementById('stop-capture').addEventListener('click', async () => {
    try {
      await api('/api/capture/stop', { method: 'POST', body: '{}' });
      await refreshCapture();
      await refreshFiles();
    } catch (err) {
      alert(err.message);
    }
  });
}

async function refreshAll() {
  const health = await api('/api/health');
  renderHealth(health);
  await refreshCapture();
  await refreshRuns();
  await refreshFiles();
  if (state.selectedRun) await refreshLog();
}

async function main() {
  document.getElementById('refresh-all').addEventListener('click', refreshAll);
  await initCaptureForm();
  await refreshScenarios();
  await refreshAll();
  setInterval(async () => {
    await refreshRuns();
    await refreshCapture();
    if (state.selectedRun) await refreshLog();
  }, 3000);
}

main().catch(err => {
  document.getElementById('log-view').textContent = err.message;
});
