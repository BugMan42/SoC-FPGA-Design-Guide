library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;


entity accelerator is
	  port (clk: in std_logic;
			nReset: in std_logic;
			-- Slave part
			avs_Add: in std_logic_vector(1 downto 0);
			avs_CS: in std_logic;
			avs_Wr: in std_logic;
			avs_Rd: in std_logic;
			avs_WrData: in std_logic_vector(31 downto 0);
			avs_RdData: out std_logic_vector(31 downto 0);
			-- Master part
			avm_Add: out std_logic_vector(31 downto 0);
			avm_BE: out std_logic_vector(3 downto 0);
			avm_Wr: out std_logic;
			avm_Rd: out std_logic;
			avm_WrData: out std_logic_vector(31 downto 0);
			avm_RdData: in std_logic_vector(31 downto 0);
			avm_WaitRequest: in std_logic);

end entity accelerator;

architecture behv of accelerator is
	-- internal registers
	signal RegAddStart: std_logic_vector(31 downto 0);
	signal RegLgt: std_logic_vector(15 downto 0); -- Max 65535 data
	signal DataRd: std_logic_vector(31 downto 0); -- Data Read from memory to use for calculus
	signal Start, Finish: std_logic; -- Command, Status bits
	signal CntAdd: unsigned(31 downto 0); -- Address in memory
	signal CntAddT: unsigned(31 downto 0); -- Address in memory + offset
	--signal CntLgt: unsigned (15 downto 0); -- Number of element to read
	
	signal cnt: integer:= 0;
	
	type T_DATA is array (3 downto 0)
        of unsigned(31 downto 0);
		  
	signal values: T_DATA := (others => X"00000000");
	signal values_ordered: T_DATA := (others => X"00000000");
	signal values_ordered_final: T_DATA := (others => X"00000000");
	
	constant n : integer := 3;
	
	-- State Machine for DMA:
	type SM is(Idle, LdParam, RdAcc, WaitRd, Calcul, PreWr, WrS, WrEnd);
	signal StateM: SM;
		
		component sort is
			PORT(	in1, in2, in3, in4: 		IN unsigned (31 DOWNTO 0);
					out1, out2, out3, out4: OUT unsigned(31 DOWNTO 0));
		end component;
	
	
	begin
	
	
		sort_0 : sort
		port map(
			in1 => values(0),
			in2 => values(1),
			in3 => values(2),
			in4 => values(3),
			out1 => values_ordered(0),
			out2 => values_ordered(1),
			out3 => values_ordered(2),
			out4 => values_ordered(3)
		);
	
	
		-- Slave Wr Access, Programmation des registres de l'interface Avalon Slave
		AvalonSlaveWr:	process(clk, nReset)
		begin
			if nReset = '0' then
				RegAddStart <= (others => '0');
				RegLgt <= (others => '0');
				Start <= '0';
			elsif rising_edge(clk) then
				Start <= '0';
				if avs_CS = '1' and avs_Wr = '1' then
					case avs_Add is
						when "00" => RegAddStart <= avs_WrData ;
						when "01" => RegLgt <= avs_WrData(15 downto 0);
						when "10" => Start <= avs_WrData(0);
						when others => null;
					end case;
				end if;
			end if;
		end process AvalonSlaveWr;

		-- Slave Rd Access, mettre 1 cycle d'attente car lecture synchrone
		AvalonSlaveRd: process(clk)
		begin
			if rising_edge(clk) then
				if avs_CS = '1' and avs_Rd = '1' then
					avs_RdData <= (others => '0'); -- etat par default
					case avs_Add is
						when "00" => avs_RdData <= RegAddStart ;
						when "01" => avs_RdData(15 downto 0) <= RegLgt;
						when "10" => avs_RdData(0) <= Start;
						when "11" => avs_RdData(0) <= Finish;
						when others => null;
					end case;
				end if;
			end if;
		end process AvalonSlaveRd;
		------------------------------------------------------------------------------------------
		-- DMA Access
		-- Ce séquenceur n'est pas optimisé,
		-- le calcul pourrait être effectué lors de la lecture des données directement
		-- Boucle pour lire toutes les données
		-- Ecrit les données calculées après les calculs



		AvalonMaster: process(clk, nReset)
		begin
			if nReset = '0' then
				Finish <= '0';
				CntAdd <= (others => '0');
				--CntLgt <= (others => '0');
				StateM <= Idle;
			elsif rising_edge(clk) then
				case StateM is
					when Idle => -- Wait Commande Start
						avm_Add <= (others => '0'); -- Init Avalon Master au repos
						avm_BE <= "0000";
						avm_Wr <= '0';
						avm_Rd <= '0';
						cnt <= 0;
					
					if Start = '1' then
						StateM <= LdParam; 
						Finish <= '0';
					end if;
					
					when LdParam => -- Charge params to transfer
						CntAdd <= unsigned(RegAddStart); -- unsigned for calculation
						StateM <= RdAcc;
						CntAddT <= unsigned(RegAddStart);
					
					when RdAcc => -- Start cycle Master READ
						avm_Add <= std_logic_vector(CntAddT);-- Address, CntAdd =>  address + counter
						avm_BE <= "1111";
						avm_Rd <= '1';
						StateM <= WaitRd;
					
					when WaitRd => -- READ
						if avm_WaitRequest = '0' then
							values(cnt) <= unsigned(avm_RdData);
							avm_BE <= "0000";
							avm_Rd <= '0';
							if (cnt < n) then
								StateM <= RdAcc;
								cnt <= cnt + 1;
								CntAddT <= CntAddT + 4;
							else	
								StateM <= Calcul;
							end if;	
							else 	
						end if;
						
					when Calcul => -- ACCELERATION
					
						
						CntAddT <= CntAdd;
						cnt <= 0;
						StateM <= PreWr;
					
					when PreWr => -- Calcul and write request						
						avm_WrData <= std_logic_vector(values_ordered(cnt));
						avm_Add <= std_logic_vector(CntAddT);
						--rise the write request
						avm_Wr <= '1';
						avm_BE <= "1111";
						StateM <= WrS;
					
					when WrS => -- Wait write end
						if avm_WaitRequest = '0' then
							avm_BE <= "0000";
							avm_Wr <= '0';
							
							if (cnt < n) then
								StateM <= PreWr;
								cnt <= cnt + 1;
								CntAddT <= CntAddT + 4;
							
							else
								Finish <= '1'; -- active bit finish
								StateM <= Idle; -- go to idle
							end if;	
							
						end if;
												
					when others => null;
				
				end case;
			end if;
		end process AvalonMaster;
end architecture behv;