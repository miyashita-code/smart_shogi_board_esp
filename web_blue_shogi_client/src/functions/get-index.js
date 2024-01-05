const cols = ["1", "2", "3", "4", "5", "6", "7", "8", "9"];
const rows = ['一', '二', '三', '四', '五', '六', '七', '八', '九'];

/**
 * Converts a shogi position string to an array of column and row indexes.
 * @param {String} shogiPos e.g. '6四';
 * @returns {Object} An object containing the column and row indexes.
 */
export const getIndex = (shogiPos) => {

    // Check that the input is a valid string.
    if (shogiPos.length !== 2) {
        return;
    }

    // Extract the column and row values from the input string.
    const shogiCol = shogiPos[0];
    const shogiRow = shogiPos[1];

    // Convert the column and row values to their corresponding indexes in the cols and rows arrays.
    const colIndex = cols.indexOf(shogiCol) + 1;
    const rowIndex = rows.indexOf(shogiRow) + 1;

    // Return an array containing the column and row indexes.
    return {x: colIndex, y: rowIndex};
}
