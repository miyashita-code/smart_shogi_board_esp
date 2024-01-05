from flask import Flask, request, jsonify
from flask_cors import CORS
import subprocess

app = Flask(__name__)
CORS(app)

@app.route('/bestmove', methods=['POST'])
def get_best_move():
    # Get the SFEN string from the request data
    sfen = request.json['sfen']

    # Start the Gikou engine process
    gikou_process = subprocess.Popen('gikou2_win\gikou.exe', stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    # Send the USI commands to the Gikou engine (depth 0 to 100)
    gikou_process.stdin.write(b'usi\nsetoption name BookFile value params.bin 00_nomal.bin\nsetoption name DepthLimit value 30\nisready\nusinewgame\n')
    gikou_process.stdin.flush()

    # Send the position and go commands to the Gikou engine
    position_command = f'position sfen {sfen}\n'.encode('utf-8')
    gikou_process.stdin.write(position_command)
    gikou_process.stdin.write(b'go\n')
    gikou_process.stdin.flush()

    # Read the best move from the Gikou engine output
    output = gikou_process.stdout.readline().decode('utf-8').strip()
    while not output.startswith('bestmove'):
        output = gikou_process.stdout.readline().decode('utf-8').strip()
    best_move = output.split()[1]

    # Close the Gikou engine process
    gikou_process.stdin.close()
    gikou_process.stdout.close()
    gikou_process.kill()

    # Return the best move as a JSON response with the CORS header
    response = {'best_move': best_move}
    return jsonify(response), 200, {'Access-Control-Allow-Origin': 'https://localhost:3000'}

if __name__ == '__main__':
    app.run()

