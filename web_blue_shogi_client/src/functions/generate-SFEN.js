// Importing required classes from the "shogi.js" library
import { Color, Shogi, Piece } from "shogi.js";

/**
 * Returns an SFEN string representing the current board state and pieces in hand
 * @param {Shogi} shogi - The current shogi game instance
 * @param {string} moveInfo - Additional move information to include in the SFEN string (e.g. "moves 1")
 * @returns {string} The SFEN string representing the current board state and pieces in hand
 */
export const generateSFEN = (shogi, moveInfo = "1") => {
  let sfenBoard = "";
  const board = shogi.board;
  const turn = shogi.turn;

  // Loop through the rows of the board (from 1st to 9th)
  for (let i = 0; i < 9; i++) {
    let emptyCount = 0;

    // Loop through the columns of the board (from 9th to 1st)
    for (let j = 8; j >= 0; j--) {
      const piece = board[j][i];

      if (!piece) {
        emptyCount++; // Increment the count of empty spaces in a row
        continue; // Move to the next column if there is no piece in the current position
      }

      // If there are empty spaces in a row before a piece, append the count to the SFEN string
      if (emptyCount > 0) {
        sfenBoard += `${emptyCount}`;
        emptyCount = 0; // Reset the count of empty spaces in a row
      }

      sfenBoard += piece.toSFENString(); // Append the SFEN string representation of the piece to the SFEN string
    }

    // When we finish processing a row, add the count of empty spaces at the end of the SFEN string (if any)
    if (emptyCount > 0) {
      sfenBoard += emptyCount;
    }

    // Add a slash between rows (except for the last row)
    if (i < 8) {
      sfenBoard += "/";
    }
  }

  let sfenHands;

  let handWhite = "";
  let handBlack = "";

  // Get the summary of the black player's pieces in hand
  const blackHandDict = shogi.getHandsSummary(Color.Black);
  // Filter out any undefined keys (i.e. keys for pieces that the player doesn't have)
  const blackHandDictKeys = Object.keys(blackHandDict).filter(element => element !== "undefined")

  // Loop through the black player's pieces in hand
  blackHandDictKeys.forEach((key) => {
    const count = blackHandDict[key]; // The number of pieces of the current type that the player has
    const color = Color.Black ;
    const piece = new Piece("+" + key); // Create a new piece object with the current type


    // Append the SFEN string representation of the piece to the player's hand
    if(count === 0) {
      // If the count is 0, skip the piece
    } 
    else if (count === 1) {
      handBlack += piece.toSFENString();
    }
    else {
      handBlack += `${count}` + piece.toSFENString();
    }
  });

  // Get the summary of the white player's pieces in hand
  const whiteHandDict = shogi.getHandsSummary(Color.White);
  // Filter out any undefined keys (i.e keys for pieces that the player doesn't have)
const whiteHandDictKeys = Object.keys(whiteHandDict).filter(element => element !== "undefined")

// Loop through the white player's pieces in hand
whiteHandDictKeys.forEach((key) => {
const count = whiteHandDict[key]; // The number of pieces of the current type that the player has
const color = Color.White;
const piece = new Piece("-" + key); // Create a new piece object with the current type


// Append the SFEN string representation of the piece to the player's hand
if(count === 0) {
  // If the count is 0, skip the piece
} 
else if (count === 1) {
  handWhite += piece.toSFENString();
}
else {
  handWhite += `${count}` + piece.toSFENString();
}});

// If both players have an empty hand, set the SFEN hands string to "-"
if (handWhite === "" && handBlack === "") {
sfenHands = "-";
}
else {
// Otherwise, concatenate the black and white hands and set the SFEN hands string to the result
sfenHands = handBlack + handWhite;
}

// Set the SFEN turn string based on whose turn it is
const sfenTurn = turn === Color.Black ? "b" : "w";

// Return the final SFEN string
return `${sfenBoard} ${sfenTurn} ${sfenHands} ${moveInfo}`;
};


