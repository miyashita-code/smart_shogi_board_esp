export class Cell {
	constructor(pos, piece) {
		this.pos = pos;
		this.piece = piece;
	}
}
//  returns an array of range 1, n
const range = (n) => {
	return Array.from({ length: n }, (_, i) => i + 1);
};
/**
 *
 * @param {String} sfenString 'lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1'
 * @returns {Cell[]} 
 */
export const createBoard = (sfenString) => {
	// Get the first portion of SFEN string
	const sfen = sfenString.split(' ')[0];

	// Remove the row delimiters '/'
	const sfenPieces = sfen.split('/').join('');

	// Create an array from SFEN pieces string
	let pieces = Array.from(sfenPieces);
	const plusCount = pieces.filter((piece) => piece === "+").length;

	for (let i = 0; i < pieces.length - plusCount - 1; i++) {
		if (pieces[i] === "+" && isNaN(parseInt(pieces[i + 1]))) {
		  // If the current character is "+" and the next character is not a number, join them.
		  pieces[i] = pieces[i] + pieces[i + 1];
		  pieces.splice(i + 1, 1);
		}
	}

	const buffer = [...pieces];


	// Replace the number in the array with an empty array containing N empty strings
	buffer.forEach((item, index) => {
		if (isFinite(item)) {
			pieces.splice(index, 1, range(item).fill(''));
		}
	});
	// Convert the 2D array into 1D array
	pieces = pieces.flat();

	// Define column and row names
	const columns = range(9)
		.map((n) => n.toString())
		.reverse(); // ["9", "8", "7", "6", "5", "4", "3", "2", "1"]
	const rows = ['一', '二', '三', '四', '五', '六', '七', '八', '九'];

	// Create an array of cells with coordinates (e.g. '9一', '８一', '7一' ..., '1九')
	const cells = []; 
	for (let i = 0; i < rows.length; i++) {
		const row = rows[i];
		for (let j = 0; j < columns.length; j++) {
			const col = columns[j];
			cells.push(col + row);
		}
	}

	// Create an array of Cell objects, where each object has a piece and a coordinate
	const board = [];
	for (let i = 0; i < cells.length; i++) {
		const cell = cells[i];
		const piece = pieces[i];
		board.push(new Cell(cell, piece));
	}

	return board;
};


/**
 * Convert hands in SFEN format to an array of dictionaries of each piece in hand.
 * @param {string} sfenString SFEN format board status
 * @returns {{}[]} Array of dictionaries of each piece in hand
 */
export const createPiecesInHands = (sfenString) => {
    // Get the hands from the SFEN string.
    const sfenHands = sfenString.split(' ')[2];

    // Create dictionaries to store the pieces in hand for each player.
    const dict_b = { R: 0, B: 0, G: 0, S: 0, N: 0, L: 0, P: 0 };
    const dict_w = { r: 0, b: 0, g: 0, s: 0, n: 0, l: 0, p: 0 };

    // If neither player has any pieces in hand, return the dictionaries as they are.
    if (sfenHands === "-") {
        return [dict_b, dict_w];
    }

    // If the first character of the hands is an alphabet, increment the count of that piece.
    if (isNaN(parseInt(sfenHands[0]))) {
        const piece = sfenHands[0];
        if (piece in dict_b) {
            dict_b[piece]++;
        } else if (piece in dict_w) {
            dict_w[piece]++;
        }
    }
    
    // Loop through the hands and update the dictionaries accordingly.
    for (let i = 0; i < sfenHands.length; i ++) {
        let count = parseInt(sfenHands[i]);
        const piece = sfenHands[i + 1];
        if (isNaN(count)) {
            count = 1;
        }

        if (piece in dict_b) {
            dict_b[piece] += count;
        } else if (piece in dict_w) {
            dict_w[piece] += count;
        }
    }
    
    // Return the updated dictionaries.
    return [dict_b, dict_w];
}