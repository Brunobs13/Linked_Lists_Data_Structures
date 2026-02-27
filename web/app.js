async function fetchJson(url, options = {}) {
  const response = await fetch(url, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });

  let payload;
  try {
    payload = await response.json();
  } catch {
    payload = { error: "Non-JSON response" };
  }

  if (!response.ok) {
    throw new Error(payload?.detail?.error || payload?.error || JSON.stringify(payload));
  }

  return payload;
}

function setText(id, value) {
  document.getElementById(id).textContent = String(value);
}

function setHealthBadge(status, db) {
  const badge = document.getElementById("healthBadge");
  badge.textContent = `Engine: ${status} | DB: ${db}`;
  badge.classList.remove("good", "bad");
  badge.classList.add(status === "ok" ? "good" : "bad");
}

function renderLogs(items) {
  const body = document.getElementById("logsBody");
  body.innerHTML = "";

  if (!items || items.length === 0) {
    const row = document.createElement("tr");
    row.innerHTML = `<td colspan="5">No pending logs.</td>`;
    body.appendChild(row);
    return;
  }

  for (const item of items) {
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${item.id}</td>
      <td>${item.level}</td>
      <td>${item.source}</td>
      <td>${item.message}</td>
      <td>${item.ingested_at_ms}</td>
    `;
    body.appendChild(row);
  }
}

async function refreshMetrics() {
  const [health, metrics, logs] = await Promise.all([
    fetchJson("/health"),
    fetchJson("/metrics"),
    fetchJson("/logs"),
  ]);

  setHealthBadge(health.status, health.db);
  setText("queueDepth", metrics.queue_depth);
  setText("processedCount", metrics.total_processed);
  setText("ingestedCount", metrics.total_ingested);
  setText("errorCount", metrics.total_errors);
  setText("memoryEstimate", `${metrics.memory_bytes_estimate} bytes`);
  setText("lastProcessing", metrics.last_processing_ms.toFixed(3));

  renderLogs(logs.items || []);
}

async function submitLog(event) {
  event.preventDefault();
  const level = document.getElementById("levelInput").value;
  const source = document.getElementById("sourceInput").value;
  const message = document.getElementById("messageInput").value;
  const statusEl = document.getElementById("formStatus");

  try {
    await fetchJson("/logs", {
      method: "POST",
      body: JSON.stringify({ level, source, message }),
    });
    statusEl.textContent = "Log submitted successfully.";
    statusEl.style.color = "#4fe2a8";
    document.getElementById("messageInput").value = "";
    await refreshMetrics();
  } catch (err) {
    statusEl.textContent = `Submission failed: ${err.message}`;
    statusEl.style.color = "#ff6b7d";
  }
}

async function triggerProcess() {
  const maxItems = Number(document.getElementById("maxProcessInput").value || 0);
  const resultEl = document.getElementById("processResult");

  try {
    const result = await fetchJson("/process", {
      method: "POST",
      body: JSON.stringify({ max_items: maxItems }),
    });
    resultEl.textContent = JSON.stringify(result, null, 2);
    await refreshMetrics();
  } catch (err) {
    resultEl.textContent = `Processing failed: ${err.message}`;
  }
}

document.getElementById("logForm").addEventListener("submit", submitLog);
document.getElementById("processBtn").addEventListener("click", triggerProcess);

refreshMetrics().catch((err) => {
  document.getElementById("healthBadge").textContent = `Startup error: ${err.message}`;
  document.getElementById("healthBadge").classList.add("bad");
});

setInterval(() => {
  refreshMetrics().catch((err) => {
    document.getElementById("healthBadge").textContent = `Refresh error: ${err.message}`;
    document.getElementById("healthBadge").classList.add("bad");
  });
}, 3000);
