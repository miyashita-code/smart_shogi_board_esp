import React from 'react';
import PropTypes from 'prop-types';
import './piece-styles.css';
import { sfenToImageName } from '../../functions/sfen-to-image-name';

const Piece = ({ name, pos }) => {
    let image;
    const imageName = sfenToImageName(name);

    try {
        image = require(`../../assets/pieces/${imageName}.png`);
    } catch (error) {
        image = require('../../assets/pieces/empty.png'); //an empty fallback image
    }

    return (
        <img
            className="piece"
            src={image}
            alt=""
        />
    );
};

Piece.prototype = {
    name: PropTypes.string.isRequired,
    pos: PropTypes.string.isRequired,
};
export default Piece;