// ESPager by Hardcore Corey Harding
const char gsc_index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESPager: GSC Encoder</title>

  <style>
  /* -------------------------------------------------
     Theme – matches the encoder‑selection page
     ------------------------------------------------- */
  :root{
    --bg-dark:#0a0a12;
    --bg-grad:#0d1a12;
    --bg-deep:#060808;
    --accent:#00ff88;
    --text-main:#33ff66;
    --text-sub:#e0e0e0;
    --panel:#16213e;
    --border:#0f3460;
  }

  body{
    background-color:var(--bg-dark);
    color:var(--text-main);
    font-family:'Courier New',monospace;
    min-height:100vh;
    margin:0;
    padding:2rem;
    text-shadow:
      0 0 2px var(--text-main),
      0 0 8px rgba(51,255,102,.4),
      0 0 20px rgba(51,255,102,.15);
  }

  /* background layers */
  body::before,
  body::after,
  .vignette,
  .glass{
    content:"";
    position:fixed;
    inset:0;
    pointer-events:none;
  }
  body::before{
    background:radial-gradient(ellipse at 50% 46%,var(--bg-grad) 0%,var(--bg-deep) 78%);
    animation:hum 5.2s steps(6) infinite;
    z-index:-2;
  }
  @keyframes hum{
    0%,100%{filter:brightness(0.96);}
    50%{filter:brightness(1.08);}
  }
  body::after{
    background:repeating-linear-gradient(
      0deg,
      rgba(0,0,0,.35) 0,
      rgba(0,0,0,.35) 1px,
      transparent 1px,
      transparent 3px);
    animation:scan-drift 2.6s linear infinite;
    z-index:-1;
  }
  @keyframes scan-drift{to{background-position:0 3px;}}
  .vignette{
    background:radial-gradient(
      ellipse at 50% 50%,
      transparent 45%,
      rgba(0,0,0,.7) 100%);
    z-index:10;
  }
  .glass{
    background:radial-gradient(
      40% 30% at 22% 8%,
      rgba(255,255,255,.04) 0%,
      transparent 60%);
    z-index:11;
  }

  h1{
    color:var(--accent);
    margin-top:0;
  }

  /* -------------------------------------------------
     Form – vertical stack (one field per line)
     ------------------------------------------------- */
  form{
    display:flex;
    flex-direction:column;
    gap:0.8rem;
    max-width:40rem;
    margin:1rem 0;
  }
  form label{
    font-weight:bold;
    color:#aaa;
  }
  form input[type=text],
  form input[type=number],
  form select{
    width:100%;
    max-width:100%;
    background:var(--panel);
    color:var(--text-sub);
    border:1px solid var(--border);
    border-radius:4px;
    padding:0.5rem 0.6rem;
    font-family:monospace;
    box-sizing:border-box;
  }
  form input:focus,
  form select:focus{
    outline:none;
    border-color:var(--accent);
  }
  form input[type=submit]{
    align-self:flex-start;
    background:var(--border);
    color:var(--accent);
    border:1px solid var(--accent);
    border-radius:4px;
    padding:0.6rem 1.2rem;
    cursor:pointer;
    font-family:monospace;
  }
  form input[type=submit]:hover{
    background:var(--accent);
    color:#1a1a2e;
  }

  @media (max-width:600px){
    body{padding:1rem;}
    h1{font-size:1.5rem;}
    form{gap:0.6rem;}
  }
  </style>
</head>

<body>
  <div class="vignette"></div>
  <div class="glass"></div>

  <h1>ESPager: GSC Encoder</h1>

  <form action="/gsc" method="GET">
    <label for="msg">MESSAGE:</label>
    <input type="text"
           id="msg"
           name="msg"
           maxlength="256"
           value="{{MSG}}"
           placeholder="Enter message"
           required>

    <label for="cap">CAPCODE:</label>
    <input type="number"
           id="cap"
           name="cap"
           min="0"
           max="999999"
           step="1"
           value="{{CAP}}"
           placeholder="0–999999"
           required>

    <label for="function_code">FUNCTION CODE:</label>
    <select id="function_code" name="function_code">
      <option value="0" {{FC_0}}>0 (A,00)</option>
      <option value="1" {{FC_1}}>1 (B,01)</option>
      <option value="2" {{FC_2}}>2 (C,10)</option>
      <option value="3" {{FC_3}}>3 (D,11)</option>
    </select>

    <label for="data_type">DATA TYPE:</label>
    <select id="data_type" name="data_type">
      <option value="1" {{DATA_TYPE_ALPHA}}>Alphanumeric</option>
      <option value="0" {{DATA_TYPE_NUMERIC}}>Numeric</option>
    </select>

    <input type="submit" value="Send GSC Page">
  </form>

  <form class="action" action="/" method="GET">
    <input type="submit" value="Return to Encoder Selection">
  </form>
</body>
</html>
)rawliteral";