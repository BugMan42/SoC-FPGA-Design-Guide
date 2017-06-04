library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.bus_input_pkg.all;


entity accelerator_32 is
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

end entity accelerator_32;

architecture behv of accelerator_32 is
	-- internal registers
	signal RegAddStart: std_logic_vector(31 downto 0);
	signal RegLgt: std_logic_vector(15 downto 0); -- Max 65535 data
	signal DataRd: std_logic_vector(31 downto 0); -- Data Read from memory to use for calculus
	signal Start, Finish: std_logic; -- Command, Status bits
	signal CntAdd: unsigned(31 downto 0); -- Address in memory
	signal CntAddT: unsigned(31 downto 0); -- Address in memory + offset
	--signal CntLgt: unsigned (15 downto 0); -- Number of element to read
	
	signal cnt: integer:= 0;
	
	signal values: bus_array(31 downto 0);
	signal values_ordered_0,values_ordered_1: bus_array(15 downto 0);
	signal values_ordered: bus_array(31 downto 0);
	signal start_0, finish_0 : std_logic := '0';
	
	constant n : integer := 31;
	
	-- State Machine for DMA:
	type SM is(Idle, LdParam, RdAcc, WaitRd, Calcul, Calcul2, PreWr, WrS, WrEnd);
	signal StateM: SM;
		
		component bitonic is
			port (input : in bus_array(15 downto 0);
					output : out bus_array(15 downto 0));
		end component;
		
		component mix is
			generic (width_in : integer := 15;
						width_out : integer := 31);
			port (input_a : in bus_array(width_in downto 0);
					input_b : in bus_array(width_in downto 0);
					output : out bus_array(width_out downto 0);
					Start : in std_logic;
					Finish : out std_logic;
					clk   : std_logic);
		end component;
		
	
	
	begin
	
	
		sort_0 : bitonic
			port map(input => values(15 downto 0),
						output => values_ordered_0);
						
		sort_1 : bitonic
			port map(input => values(31 downto 16),
						output => values_ordered_1);	
						
		mix_0 : mix
		generic map (width_in => 15,
						 width_out => 31)
		port map(input_a => values_ordered_0,
					input_b => values_ordered_1,
					output => values_ordered,
					Start => start_0,
					Finish => finish_0,
					clk => clk);			
		
					
	
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
						start_0 <= '1';
						if (finish_0 <= '0') then 
							StateM <= Calcul2;
						end if;
						
					when Calcul2 => -- ACCELERATION
						if (finish_0 = '1') then
							start_0 <= '1';
							CntAddT <= CntAdd;
							cnt <= 0;
							StateM <= PreWr;
						else
							StateM <= Calcul2;
						end if;	
					
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