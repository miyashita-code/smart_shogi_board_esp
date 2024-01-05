/**
 * Convert a piece name in SFEN notation to an image file name
 * @param {string} sfenName - The SFEN notation of a piece name
 * @returns {string} - The file name of the image for the given piece
 */
export const sfenToImageName = (sfenName) => {
    let buf = '';
    let name = '';
    let tail = '';

    // deal with empty piece
    if (!sfenName) {
        return sfenName
    }

    // deal with Kg, kg (different type of king piece : "GYOKU")
    if (sfenName.length === 3) {
        tail = 'g';
    }
    
    // Check for a plus or minus sign at the beginning
    if (sfenName[0] === '+' || sfenName[0] === '-') {
      buf = sfenName[0];
      name = sfenName[1];
    } else {
      name = sfenName[0];
    }
    
    //console.log("sfenName", sfenName);
    //console.log("name", name);
    const color = name === name.toUpperCase() ? 'b' : 'w';
    
    // Join the pieces together
    const imageName = color + buf + name.toUpperCase() + tail;
    
    return imageName;
  };
  