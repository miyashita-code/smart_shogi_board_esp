import React from 'react';
import './cell-styles.css';
import Piece from '../piece';
import PropTypes from 'prop-types';
import {Cell as BoardCell, getCsaKindFromSfen, getIndex} from '../../functions';

const Cell = ({ cell, handleBoardCellClick, isHighLight}) => {
    return (
    <div 
        className={`cell ${isHighLight && 'hilighted'}` }
        onClick={() => {handleBoardCellClick(getIndex(cell.pos), getCsaKindFromSfen(cell.piece));console.log("cell.pos is ",cell.pos)}}>
        <Piece pos={cell.pos} name={cell.piece} />
    </div>);
};
Cell.prototype = {
    cell: PropTypes.instanceOf(BoardCell).isRequired,
    index: PropTypes.number.isRequired,
};
export default Cell;