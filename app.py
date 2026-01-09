from flask import Flask, request, render_template_string
import threading

app = Flask(__name__)

# === 全局计数变量 ===
question_count = 0

# === HTML 模板（网页面板） ===
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>ChatGPT 提问次数统计面板</title>
    <style>
        body {
            background-color: #121212;
            color: #00ff99;
            font-family: 'Consolas', 'Menlo', monospace;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
            margin: 0;
        }
        h1 { font-size: 2em; margin-bottom: 0.2em; }
        h2 { font-size: 5em; color: #00ffcc; margin: 0; }
        .footer { margin-top: 2em; font-size: 0.8em; color: #888; }
    </style>
    <script>
        async function updateCount() {
            try {
                const res = await fetch('/status');
                const text = await res.text();
                document.getElementById('count').innerText = text.match(/\\d+/)[0];
            } catch (e) {
                console.error('更新失败', e);
            }
        }
        setInterval(updateCount, 1000); // 每秒刷新
        window.onload = updateCount;
    </script>
</head>
<body>
    <h1>📊 今日 ChatGPT 提问次数</h1>
    <h2 id="count">0</h2>
    <div class="footer">自动刷新中（每秒更新一次）</div>
</body>
</html>
"""

# === 路由定义 ===
@app.route('/')
def home():
    return "✅ ChatGPT 提问统计服务已启动。访问 <a href='/panel'>/panel</a> 查看实时统计。"

@app.route('/update_count')
def update_count():
    global question_count
    value = request.args.get('value')

    if not value:
        return "❌ 请使用 /update_count?value=数字"

    try:
        question_count = int(value)
    except ValueError:
        return "❌ 参数必须是数字"

    print(f"📊 收到更新：当前次数 = {question_count}")
    return f"✅ 当前次数 = {question_count}"

@app.route('/status')
def status():
    return f"{question_count}"

@app.route('/panel')
def panel():
    return render_template_string(HTML_TEMPLATE)

# === 启动服务 ===
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080, debug=True)
