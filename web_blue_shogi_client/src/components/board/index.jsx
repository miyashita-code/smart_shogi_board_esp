import React from 'react';
import './board-styles.css';
import Cell from '../cell';
import PropTypes from 'prop-types';
import { Color } from 'shogi.js';
import { getCsaKindFromSfen } from '../../functions';


const Board = ({ cells, hands ,handleBoardCellClick, handleHandCellClick, selectedPos}) => {
    return (
        <div className="main-board">
            <div className='hand'>
                {Object.entries(hands[1]).map(([kind, amount]) => (
                    <div key={kind} onClick={() => handleHandCellClick(Color.White, getCsaKindFromSfen(kind))}>{kind},{amount}</div>)
                )}
            </div>
            <div className='board'>
                <div className='boder-container'>
                    {cells.map((cell, index) => {
                        const x = 8 - index % 9;
                        const y = Math.floor(index / 9);

                        let isHighLight = false;

                        if (selectedPos) {
                            if(selectedPos.x - 1 === x && selectedPos.y - 1 === y) {
                                isHighLight = true;
                            }
                        }
                        
                        return (
                            <Cell cell={cell} pos={cell.pos} handleBoardCellClick={handleBoardCellClick} isHighLight={isHighLight}/>
                        );
}                   )}
                </div>
            </div>
            <div className='hand'>
                {Object.entries(hands[0]).map(([kind, amount]) => {
                    return (<div key={kind} onClick={() => handleHandCellClick(Color.Black, getCsaKindFromSfen(kind))}>{kind},{amount}</div>)
                })}
            </div>
        </div>
    );
};

Board.prototype = {
    cells: PropTypes.array.isRequired,
    hands: PropTypes.array.isRequired,
};


export default Board;
