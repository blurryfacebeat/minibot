
import { createServer } from "node:http";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { join } from "node:path";

const PORT = 3000;

// IP робота из Serial Monitor
const ROBOT_IP = process.env.ROBOT_IP || "192.168.0.98";
const ROBOT_URL = `http://${ROBOT_IP}`;

const SAMPLE_RATE = 16000;
const DURATION_SECONDS = 5;
const CHANNELS = 1;
const BITS_PER_SAMPLE = 16;

const EXPECTED_BYTES =
  SAMPLE_RATE *
  DURATION_SECONDS *
  CHANNELS *
  (BITS_PER_SAMPLE / 8);

const RECORDINGS_DIR = join(process.cwd(), "recordings");
const WAV_PATH = join(RECORDINGS_DIR, "latest.wav");

let recording = false;

// Преобразуем сырой PCM в WAV
function createWav(pcm: Buffer): Buffer {
  const header = Buffer.alloc(44);

  const byteRate =
    SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8);

  const blockAlign =
    CHANNELS * (BITS_PER_SAMPLE / 8);

  header.write("RIFF", 0);
  header.writeUInt32LE(36 + pcm.length, 4);
  header.write("WAVE", 8);

  header.write("fmt ", 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20); // PCM
  header.writeUInt16LE(CHANNELS, 22);
  header.writeUInt32LE(SAMPLE_RATE, 24);
  header.writeUInt32LE(byteRate, 28);
  header.writeUInt16LE(blockAlign, 32);
  header.writeUInt16LE(BITS_PER_SAMPLE, 34);

  header.write("data", 36);
  header.writeUInt32LE(pcm.length, 40);

  return Buffer.concat([header, pcm]);
}

// Удобный ответ JSON
function json(
  res: import("node:http").ServerResponse,
  status: number,
  data: unknown
) {
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });

  res.end(JSON.stringify(data));
}

// Допустимые команды для ESP32
const commands = new Set([
  "boot",
  "wake",
  "idle",
  "listen",
  "think",
  "speak",
  "sleep",
  "blink",
  "left",
  "right",
  "up",
  "down",
  "center",
  "pupils on",
  "pupils off",
]);

// Читаем небольшой JSON-запрос от браузера
async function readBody(
  req: import("node:http").IncomingMessage
): Promise<string> {
  let body = "";

  for await (const chunk of req) {
    body += chunk.toString();

    if (body.length > 4096) {
      throw new Error("Request too large");
    }
  }

  return body;
}

// HTML-панель
const PAGE = `
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport"
        content="width=device-width, initial-scale=1">

  <title>MiniBot Studio</title>

  <style>
    * { box-sizing: border-box; }

    body {
      margin: 0;
      background: #101218;
      color: #fff;
      font-family: system-ui, sans-serif;
      padding: 24px;
    }

    main {
      max-width: 560px;
      margin: 0 auto;
    }

    h1 { margin-bottom: 8px; }

    p {
      color: #9ca3af;
      line-height: 1.5;
    }

    .card {
      background: #1c202a;
      border: 1px solid #313845;
      border-radius: 16px;
      padding: 20px;
      margin-top: 20px;
    }

    button {
      background: #2c3442;
      color: white;
      border: 1px solid #434d60;
      border-radius: 12px;
      padding: 14px;
      font-size: 15px;
      cursor: pointer;
    }

    button:disabled {
      opacity: .5;
      cursor: wait;
    }

    .primary {
      background: #2563eb;
      border-color: #3b82f6;
      width: 100%;
      font-size: 18px;
      padding: 20px;
    }

    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin-top: 16px;
    }

    audio {
      width: 100%;
      margin-top: 20px;
    }

    #status {
      color: #a7f3d0;
      margin-top: 18px;
      min-height: 24px;
    }

    #error {
      color: #fca5a5;
      margin-top: 12px;
    }
  </style>
</head>

<body>
  <main>
    <h1>🤖 MiniBot Studio</h1>
    <p>Windows + ESP32 + INMP441</p>

    <div class="card">
      <h2>🎤 Запись голоса</h2>

      <p>
        Нажми кнопку и говори в микрофон MiniBot
        обычным голосом примерно с 20–30 см.
      </p>

      <button id="record" class="primary"
              onclick="recordAudio()">
        ● Записать 5 секунд
      </button>

      <div id="status">Готов к записи</div>
      <div id="error"></div>

      <audio id="player" controls></audio>

      <p>
        Запись сохраняется на Windows
        в файле recordings/latest.wav.
      </p>
    </div>

    <div class="card">
      <h2>👀 Управление глазами</h2>

      <div class="grid">
        <button onclick="sendCommand('boot')">Проснуться</button>
        <button onclick="sendCommand('sleep')">Спать</button>
        <button onclick="sendCommand('idle')">Обычный</button>
        <button onclick="sendCommand('listen')">Слушает</button>
        <button onclick="sendCommand('think')">Думает</button>
        <button onclick="sendCommand('speak')">Говорит</button>
        <button onclick="sendCommand('left')">Влево</button>
        <button onclick="sendCommand('right')">Вправо</button>
        <button onclick="sendCommand('center')">Центр</button>
        <button onclick="sendCommand('blink')">Моргнуть</button>
        <button onclick="sendCommand('pupils on')">Зрачки ВКЛ</button>
        <button onclick="sendCommand('pupils off')">Зрачки ВЫКЛ</button>
      </div>
    </div>
  </main>

  <script>
    async function recordAudio() {
      const button = document.getElementById('record');
      const status = document.getElementById('status');
      const error = document.getElementById('error');
      const player = document.getElementById('player');

      button.disabled = true;
      status.textContent = 'Идёт запись...';
      error.textContent = '';

      try {
        const response = await fetch('/api/record', {
          method: 'POST'
        });

        const result = await response.json();

        if (!response.ok) {
          throw new Error(result.error || 'Recording failed');
        }

        player.src =
          '/recordings/latest.wav?t=' + Date.now();

        player.load();

        status.textContent =
          'Готово! Запись сохранена. Нажми Play.';
      } catch (err) {
        error.textContent = err.message;
        status.textContent = 'Ошибка записи';
      } finally {
        button.disabled = false;
      }
    }

    async function sendCommand(command) {
      try {
        const response = await fetch('/api/command', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({ command })
        });

        if (!response.ok) {
          throw new Error(await response.text());
        }
      } catch (err) {
        document.getElementById('error')
          .textContent = err.message;
      }
    }
  </script>
</body>
</html>
`;

// Создаём сервер Node.js
const server = createServer(async (req, res) => {
  const pathname = new URL(
    req.url || "/",
    "http://localhost"
  ).pathname;

  try {
    // Отдаём нашу веб-панель
    if (req.method === "GET" && pathname === "/") {
      res.writeHead(200, {
        "Content-Type": "text/html; charset=utf-8",
      });

      res.end(PAGE);
      return;
    }

    // Запись аудио
    if (
      req.method === "POST" &&
      pathname === "/api/record"
    ) {
      if (recording) {
        json(res, 409, {
          error: "Recording already in progress",
        });
        return;
      }

      recording = true;

      try {
        console.log("Requesting audio from MiniBot...");

        const response = await fetch(
          `${ROBOT_URL}/api/record`,
          {
            method: "POST",
            signal: AbortSignal.timeout(20000),
          }
        );

        if (!response.ok) {
          throw new Error(
            `ESP32 returned HTTP ${response.status}`
          );
        }

        const pcm = Buffer.from(
          await response.arrayBuffer()
        );

        console.log(`Received ${pcm.length} bytes`);

        if (pcm.length !== EXPECTED_BYTES) {
          throw new Error(
            `Invalid audio length: ${pcm.length}, ` +
            `expected ${EXPECTED_BYTES}`
          );
        }

        const wav = createWav(pcm);

        await mkdir(RECORDINGS_DIR, {
          recursive: true,
        });

        await writeFile(WAV_PATH, wav);

        console.log("Saved:", WAV_PATH);

        json(res, 200, {
          ok: true,
          bytes: pcm.length,
          duration: DURATION_SECONDS,
        });
      } finally {
        recording = false;
      }

      return;
    }

    // Управление глазами
    if (
      req.method === "POST" &&
      pathname === "/api/command"
    ) {
      const body = JSON.parse(await readBody(req));

      const command = body.command;

      if (
        typeof command !== "string" ||
        !commands.has(command)
      ) {
        json(res, 400, {
          error: "Unknown command",
        });

        return;
      }

      const response = await fetch(
        `${ROBOT_URL}/api/command`,
        {
          method: "POST",
          headers: {
            "Content-Type":
              "application/x-www-form-urlencoded",
          },
          body: new URLSearchParams({
            cmd: command,
          }),
          signal: AbortSignal.timeout(10000),
        }
      );

      if (!response.ok) {
        json(res, 502, {
          error: `ESP32 returned ${response.status}`,
        });
        return;
      }

      json(res, 200, { ok: true });
      return;
    }

    // Воспроизведение последней записи
    if (
      req.method === "GET" &&
      pathname === "/recordings/latest.wav"
    ) {
      let file: Buffer;

      try {
        file = await readFile(WAV_PATH);
      } catch {
        json(res, 404, {
          error: "No recording yet",
        });
        return;
      }

      res.writeHead(200, {
        "Content-Type": "audio/wav",
        "Content-Length": file.length,
        "Cache-Control": "no-store",
      });

      res.end(file);
      return;
    }

    json(res, 404, {
      error: "Not found",
    });
  } catch (error) {
    console.error(error);

    if (!res.headersSent) {
      json(res, 500, {
        error:
          error instanceof Error
            ? error.message
            : "Unknown error",
      });
    } else {
      res.end();
    }
  }
});

server.listen(PORT, "0.0.0.0", () => {
  console.log("");
  console.log("🤖 MiniBot Studio started!");
  console.log(`Web: http://localhost:${PORT}`);
  console.log(`Robot: ${ROBOT_URL}`);
  console.log("");
});
