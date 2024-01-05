
import React, { useState, useRef, useEffect } from 'react';
import { Shogi, Piece, Color } from "shogi.js";
import { createBoard, createPiecesInHands, generateSFEN} from '../../functions/';
import Board from '../../components/board';
import "./game-styles.css";

const SFEN = 'lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1'; // Init SFEN positionconst 
const Game = () => {
  const [sfen, setSfen] = useState(SFEN);
  const { current: shogi } = useRef(new Shogi());
  const [board, setBoard] = useState(createBoard(sfen));
  const [hands, setHands] = useState(createPiecesInHands(sfen));
  const [turn, setTurn] = useState(undefined);
  const [isMoving, setIsMoving] = useState(false);
  const [fromPos, setFromPos] = useState(undefined);
  const [fromKind, setFromKind] = useState(undefined);
  const [selectedKindFromHand, setSelectedKindFromHand] = useState(undefined);


  // init
  useEffect(() => {
    shogi.initializeFromSFENString(sfen);
    setTurn(shogi.turn);
  }, [sfen, shogi]);

  // update board and hands state as sfen is updated
  useEffect(() => {
    setBoard(createBoard(sfen));
    setHands(createPiecesInHands(sfen));
    setTurn(shogi.turn);
  }, [sfen]);


  /**
   * 
   * @param {x,y} pos 
   */
  const makeMove = (pos, promotion = false) => {
      const from = fromPos;
      const to = pos;

      try {
        shogi.move(from.x, from.y, to.x, to.y, promotion);
        setSfen(generateSFEN(shogi));
        return true;
      } catch(e) {
        console.log(e);
        return false;
      }
  };

  const makeDrop = (pos, kind) => {
    try {
      shogi.drop(pos.x, pos.y, kind);
      setSfen(generateSFEN(shogi));
    } catch(e) {
      console.log(e);
    }
  }

  /**
   * 
   * @param {{x: number, y: number}} fromPos 
   * @param {{x: number, y:number}} toPos 
   * @returns Boolean of if the piece inside of promotion area or not.
   */
  const isInsidePromotionArea = (fromPos, toPos) => {

    console.log("1 <= y <= 3", (1 <= fromPos.y && fromPos.y <= 3),(1 <= toPos.y && toPos.y <= 3));
    console.log("7 <= y <= 9", (7 <= fromPos.y && fromPos.y <= 9),(7 <= toPos.y && toPos.y <= 9));
    // x, y is 1 index number
    if (shogi.turn === Color.Black) {
      return ((1 <= fromPos.y && fromPos.y <= 3) || (1 <= toPos.y && toPos.y <= 3));
    }
    else {
      return ((7 <= fromPos.y && fromPos.y <= 9) || (7 <= toPos.y && toPos.y <= 9));
    }
  }

  
  /**
   * 
   * @param {{x: Number, y: Number}} pos 
   */
  const handleBoardCellClick = (pos, kind) => {
    // TODO
    // 可能な動きのリストに無ければ, 無視

    if (!isMoving) {
      setFromPos(pos);
      setFromKind(kind);
      setIsMoving(true);

      // TODO
      // if opponent piece is selected , ignore

    }
    else if (selectedKindFromHand) {
      makeDrop(pos, selectedKindFromHand);
      setSelectedKindFromHand(undefined);
    }
    else if (Piece.canPromote(fromKind) && !Piece.isPromoted(fromKind) && isInsidePromotionArea(fromPos, pos)) {
      console.log(fromKind, Piece.canPromote(fromKind) ,!Piece.isPromoted(fromKind) ,isInsidePromotionArea(fromPos, pos));
      if (window.confirm("成りますか？")) {
        makeMove(pos, true);
      }
      else {
        makeMove(pos);
      }
      
      setIsMoving(false);
      setFromPos(undefined);
      setFromKind(undefined);
    }
    else {
      console.log(fromKind, Piece.canPromote(fromKind) ,!Piece.isPromoted(fromKind) ,isInsidePromotionArea(fromPos, pos));
      const result = makeMove(pos);
      console.log("result", result);
      setIsMoving(false);
      setFromPos(undefined);
      setFromKind(undefined);
    }
  }

  /**
   * 
   * @param {Color} color 
   * @param {Piece.kind} kind 
   */
  const handleHandCellClick = (color, kind) => {
    console.log(color, kind);
    if (!kind) {
      return;
    }

    if(color === shogi.turn) {
      setSelectedKindFromHand(kind);
      setIsMoving(true);
    }
    else {
      setSelectedKindFromHand(undefined);
      setIsMoving(false);
    }
  }



return (
  <div className='board-container'>
    <Board 
      cells={board} 
      hands={hands} 
      handleBoardCellClick={handleBoardCellClick} 
      handleHandCellClick={handleHandCellClick}
      selectedPos={fromPos}
    />
    <div>ターン: {turn === Color.Black ? "▲" : "▽"}</div>
  </div>);
};

export default Game;
