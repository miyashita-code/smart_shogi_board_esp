
/**
 * 
 * @param {String} sfenKind 
 * @returns CSA fromta kind of piece
 */
export const getCsaKindFromSfen = (sfenKind) => {
    let csaKind;
    if (sfenKind === "p" || sfenKind === "P") {
        csaKind = "FU";
    }
    else if (sfenKind === "l" || sfenKind === "L") {
        csaKind = "KY";
    }
    else if (sfenKind === "n" || sfenKind === "N") {
        csaKind = "KE";
    }
    else if (sfenKind === "s" || sfenKind === "S") {
        csaKind = "GI";
    }
    else if (sfenKind === "g" || sfenKind === "G") {
        csaKind = "KI";
    }
    else if (sfenKind === "b" || sfenKind === "B") {
        csaKind = "KA";
    }
    else if (sfenKind === "r" || sfenKind === "R") {
        csaKind = "HI";
    }
    else if (sfenKind === "kg" || sfenKind === "Kg" || sfenKind === "k" || sfenKind === "K") {
        csaKind = "OU";
    }
    else if (sfenKind === "+p" || sfenKind === "+P") {
        csaKind = "TO";
    }
    else if (sfenKind === "+l" || sfenKind === "+L") {
        csaKind = "NY";
    }
    else if (sfenKind === "+n" || sfenKind === "+N") {
        csaKind = "NK";
    }
    else if (sfenKind === "+s" || sfenKind === "+S") {
        csaKind = "NG";
    }
    else if (sfenKind === "+b" || sfenKind === "+B") {
        csaKind = "UM";
    }
    else if (sfenKind === "+r" || sfenKind === "+R") {
        csaKind = "RY";
    }
    else {
        console.log("NOOOOOOOO");
    }

    return csaKind;
}
