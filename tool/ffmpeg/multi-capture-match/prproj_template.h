// The parts of a Premiere Pro project that are the same in any project: the project itself, with the
// state of its panels, and its settings, from a project Premiere Pro 26.3 saved (project version 45).
// @SEQUENCE@, @PROJECT@ and @ROOT@ are put in when it is written. They use ObjectID 1 to 39.
#ifndef MCM_PRPROJ_TEMPLATE_H
#define MCM_PRPROJ_TEMPLATE_H

static const char PRPROJ_PROJECT[] = R"PRPROJ(	<Project ObjectID="1" ClassID="62ad66dd-0dcd-42da-a660-6d8fbde94876" Version="45">
		<Node Version="1">
			<Properties Version="1">
				<ProjectViewState.List ObjectID="2" ClassID="aab0946f-7a21-4425-8908-fafa2119e30e" Version="3">
					<ProjectViewStates Version="1">
						<ProjectViewState Version="1" Index="0">
							<First>a594d17e-cd6c-4480-a86c-a9aa6d98a473</First>
							<Second ObjectRef="1"/>
						</ProjectViewState>
					</ProjectViewStates>
					<ProjectViewState ObjectID="1" ClassID="18fb911d-4f21-4b7b-b196-b250dad79838" Version="3">
						<ProjectViewState.ID>a594d17e-cd6c-4480-a86c-a9aa6d98a473</ProjectViewState.ID>
						<ProjectViewState.OriginalID>00000000-0000-0000-0000-000000000000</ProjectViewState.OriginalID>
						<ProjectViewState.BinID>1000000</ProjectViewState.BinID>
						<ProjectViewState.ViewHidden>false</ProjectViewState.ViewHidden>
						<PreviewView.Visible>false</PreviewView.Visible>
						<ContentView.LastViewed>0</ContentView.LastViewed>
						<IconView.Thumbnail.Size>200</IconView.Thumbnail.Size>
						<FreeformView.Scale>1</FreeformView.Scale>
						<ListView.Thumbnail.Size>0</ListView.Thumbnail.Size>
						<IconView.Thumbnail.State>true</IconView.Thumbnail.State>
						<ListView.Thumbnail.State>false</ListView.Thumbnail.State>
						<Thumbnail.ShowsEffects.State>true</Thumbnail.ShowsEffects.State>
						<Sort.Type>0</Sort.Type>
						<Sort.Enabled>true</Sort.Enabled>
						<Sort.ColumnIndex>1</Sort.ColumnIndex>
						<Sort.Direction>0</Sort.Direction>
						<ListView.NameColumnWidth>0</ListView.NameColumnWidth>
						<IconSort.Type>0</IconSort.Type>
						<IconSort.Direction>0</IconSort.Direction>
						<IconSort.ColumnIndex>0</IconSort.ColumnIndex>
						<SideNav.Visible>true</SideNav.Visible>
						<SideNav.Width>160</SideNav.Width>
						<Project.IsEAProject>false</Project.IsEAProject>
						<ColumnListContents.Version>18</ColumnListContents.Version>
						<Columns.List ObjectRef="2"/>
					</ProjectViewState>
					<ColumnList ObjectID="2" ClassID="a1c709cd-35df-4821-8200-03565d374155" Version="1">
						<Columns Version="1">
							<Column Index="0" ObjectRef="3"/>
							<Column Index="1" ObjectRef="4"/>
							<Column Index="2" ObjectRef="5"/>
							<Column Index="3" ObjectRef="6"/>
							<Column Index="4" ObjectRef="7"/>
							<Column Index="5" ObjectRef="8"/>
							<Column Index="6" ObjectRef="9"/>
							<Column Index="7" ObjectRef="10"/>
							<Column Index="8" ObjectRef="11"/>
							<Column Index="9" ObjectRef="12"/>
							<Column Index="10" ObjectRef="13"/>
							<Column Index="11" ObjectRef="14"/>
							<Column Index="12" ObjectRef="15"/>
							<Column Index="13" ObjectRef="16"/>
							<Column Index="14" ObjectRef="17"/>
							<Column Index="15" ObjectRef="18"/>
							<Column Index="16" ObjectRef="19"/>
							<Column Index="17" ObjectRef="20"/>
							<Column Index="18" ObjectRef="21"/>
							<Column Index="19" ObjectRef="22"/>
							<Column Index="20" ObjectRef="23"/>
							<Column Index="21" ObjectRef="24"/>
							<Column Index="22" ObjectRef="25"/>
							<Column Index="23" ObjectRef="26"/>
							<Column Index="24" ObjectRef="27"/>
							<Column Index="25" ObjectRef="28"/>
							<Column Index="26" ObjectRef="29"/>
							<Column Index="27" ObjectRef="30"/>
							<Column Index="28" ObjectRef="31"/>
							<Column Index="29" ObjectRef="32"/>
							<Column Index="30" ObjectRef="33"/>
							<Column Index="31" ObjectRef="34"/>
							<Column Index="32" ObjectRef="35"/>
							<Column Index="33" ObjectRef="36"/>
							<Column Index="34" ObjectRef="37"/>
							<Column Index="35" ObjectRef="38"/>
							<Column Index="36" ObjectRef="39"/>
							<Column Index="37" ObjectRef="40"/>
							<Column Index="38" ObjectRef="41"/>
							<Column Index="39" ObjectRef="42"/>
							<Column Index="40" ObjectRef="43"/>
							<Column Index="41" ObjectRef="44"/>
							<Column Index="42" ObjectRef="45"/>
							<Column Index="43" ObjectRef="46"/>
							<Column Index="44" ObjectRef="47"/>
							<Column Index="45" ObjectRef="48"/>
							<Column Index="46" ObjectRef="49"/>
							<Column Index="47" ObjectRef="50"/>
							<Column Index="48" ObjectRef="51"/>
							<Column Index="49" ObjectRef="52"/>
							<Column Index="50" ObjectRef="53"/>
							<Column Index="51" ObjectRef="54"/>
							<Column Index="52" ObjectRef="55"/>
							<Column Index="53" ObjectRef="56"/>
							<Column Index="54" ObjectRef="57"/>
							<Column Index="55" ObjectRef="58"/>
							<Column Index="56" ObjectRef="59"/>
							<Column Index="57" ObjectRef="60"/>
						</Columns>
					</ColumnList>
					<LabelColumn ObjectID="3" ClassID="0b8cc011-65dd-4b47-aad9-751ca2891f4a" Version="1">
						<Column.Name>Label</Column.Name>
						<Column.ID>Column.PropertyText.Label</Column.ID>
						<Column.Type>17</Column.Type>
						<Column.Class>1</Column.Class>
						<Column.Width>26</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</LabelColumn>
					<SelectedItemsColumn ObjectID="4" ClassID="88bcfb15-97a7-49ed-ac05-7d3ce637d2a0" Version="1">
						<Column.Name>Selected</Column.Name>
						<Column.ID>Column.PropertyText.SelectedItems</Column.ID>
						<Column.Type>0</Column.Type>
						<Column.Class>1</Column.Class>
						<Column.Width>26</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</SelectedItemsColumn>
					<NameColumn ObjectID="5" ClassID="0547b302-c849-46b3-ae2a-b245e9dd59eb" Version="1">
						<Column.Name>Name</Column.Name>
						<Column.ID>Column.Intrinsic.Name</Column.ID>
						<Column.Type>24</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>200</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</NameColumn>
					<StringColumn ObjectID="6" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Media Type</Column.Name>
						<Column.ID>Column.Intrinsic.MediaType</Column.ID>
						<Column.Type>23</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="7" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Frame Rate</Column.Name>
						<Column.ID>Column.Intrinsic.MediaTimebase</Column.ID>
						<Column.Type>22</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</StringColumn>
					<TimecodeColumn ObjectID="8" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Media Start</Column.Name>
						<Column.ID>Column.Intrinsic.MediaStart</Column.ID>
						<Column.Type>21</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="9" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Media End</Column.Name>
						<Column.ID>Column.Intrinsic.MediaEnd</Column.ID>
						<Column.Type>20</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="10" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Media Duration</Column.Name>
						<Column.ID>Column.Intrinsic.MediaDuration</Column.ID>
						<Column.Type>19</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="11" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Video In Point</Column.Name>
						<Column.ID>Column.Intrinsic.VideoInPoint</Column.ID>
						<Column.Type>35</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="12" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Video Out Point</Column.Name>
						<Column.ID>Column.Intrinsic.VideoOutPoint</Column.ID>
						<Column.Type>36</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="13" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Video Duration</Column.Name>
						<Column.ID>Column.Intrinsic.VideoDuration</Column.ID>
						<Column.Type>33</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="14" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Audio In Point</Column.Name>
						<Column.ID>Column.Intrinsic.AudioInPoint</Column.ID>
						<Column.Type>3</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="15" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Audio Out Point</Column.Name>
						<Column.ID>Column.Intrinsic.AudioOutPoint</Column.ID>
						<Column.Type>4</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="16" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Audio Duration</Column.Name>
						<Column.ID>Column.Intrinsic.AudioDuration</Column.ID>
						<Column.Type>1</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="17" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Subclip Start</Column.Name>
						<Column.ID>Column.Intrinsic.SubclipStart</Column.ID>
						<Column.Type>39</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="18" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Subclip End</Column.Name>
						<Column.ID>Column.Intrinsic.SubclipEnd</Column.ID>
						<Column.Type>40</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="19" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Subclip Duration</Column.Name>
						<Column.ID>Column.Intrinsic.SubclipDuration</Column.ID>
						<Column.Type>41</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</TimecodeColumn>
					<StringColumn ObjectID="20" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Video Info</Column.Name>
						<Column.ID>Column.Intrinsic.VideoInfo</Column.ID>
						<Column.Type>34</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="21" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Audio Info</Column.Name>
						<Column.ID>Column.Intrinsic.AudioInfo</Column.ID>
						<Column.Type>2</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>150</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="22" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Video Usage</Column.Name>
						<Column.ID>Column.Intrinsic.VideoUsage</Column.ID>
						<Column.Type>38</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="23" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Audio Usage</Column.Name>
						<Column.ID>Column.Intrinsic.AudioUsage</Column.ID>
						<Column.Type>6</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<EditTextColumn ObjectID="24" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Tape Name</Column.Name>
						<Column.ID>Column.Intrinsic.TapeName</Column.ID>
						<Column.Type>30</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="25" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Description</Column.Name>
						<Column.ID>Column.PropertyText.Description</Column.ID>
						<Column.Type>15</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="26" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Comment</Column.Name>
						<Column.ID>Column.PropertyText.Comment</Column.ID>
						<Column.Type>10</Column.Type>
						<Column.Class>1</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="27" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Log Note</Column.Name>
						<Column.ID>Column.Intrinsic.LogNote</Column.ID>
						<Column.Type>18</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</EditTextColumn>
					<StringColumn ObjectID="28" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Media File Path</Column.Name>
						<Column.ID>Column.Intrinsic.FilePath</Column.ID>
						<Column.Type>16</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="29" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Status</Column.Name>
						<Column.ID>Column.PropertyText.Status</Column.ID>
						<Column.Type>29</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="30" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Offline Properties</Column.Name>
						<Column.ID>Column.PropertyText.OfflineProperties</Column.ID>
						<Column.Type>25</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="31" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Media File Name</Column.Name>
						<Column.ID>Column.Intrinsic.FileName</Column.ID>
						<Column.Type>58</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<EditTextColumn ObjectID="32" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Scene</Column.Name>
						<Column.ID>Column.PropertyText.Scene</Column.ID>
						<Column.Type>27</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="33" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Shot</Column.Name>
						<Column.ID>Column.PropertyText.Shot</Column.ID>
						<Column.Type>28</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="34" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Client</Column.Name>
						<Column.ID>Column.PropertyText.Client</Column.ID>
						<Column.Type>9</Column.Type>
						<Column.Class>1</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<BoolPropertyColumn ObjectID="35" ClassID="1d4dd772-4985-4f43-874a-84b2b566e724" Version="1">
						<Column.Name>Good</Column.Name>
						<Column.ID>Column.PropertyBool.Good</Column.ID>
						<Column.Type>0</Column.Type>
						<Column.Class>2</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
						<Column.Property.Key>Column.PropertyBool.Good</Column.Property.Key>
						<Column.Editable.Key>true</Column.Editable.Key>
					</BoolPropertyColumn>
					<BoolPropertyColumn ObjectID="36" ClassID="1d4dd772-4985-4f43-874a-84b2b566e724" Version="1">
						<Column.Name>Hide</Column.Name>
						<Column.ID>Column.PropertyBool.Hide</Column.ID>
						<Column.Type>0</Column.Type>
						<Column.Class>2</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
						<Column.Property.Key>Column.PropertyBool.Hide</Column.Property.Key>
						<Column.Editable.Key>true</Column.Editable.Key>
					</BoolPropertyColumn>
					<TimecodeColumn ObjectID="37" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Sound Timecode</Column.Name>
						<Column.ID>Column.Intrinsic.SoundTimeCode</Column.ID>
						<Column.Type>42</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="38" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Linear Timecode</Column.Name>
						<Column.ID>Column.Intrinsic.LinearTimeCode</Column.ID>
						<Column.Type>64</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</TimecodeColumn>
					<TimecodeColumn ObjectID="39" ClassID="9c9279d2-355c-487b-b644-0698b42e32f9" Version="1">
						<Column.Name>Aux Timecode</Column.Name>
						<Column.ID>Column.Intrinsic.AuxTimeCode</Column.ID>
						<Column.Type>65</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</TimecodeColumn>
					<EditTextColumn ObjectID="40" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Sound Roll</Column.Name>
						<Column.ID>Column.PropertyText.SoundRoll</Column.ID>
						<Column.Type>43</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="41" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Camera Roll</Column.Name>
						<Column.ID>Column.PropertyText.FilmCameraRoll</Column.ID>
						<Column.Type>47</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="42" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Daily Roll</Column.Name>
						<Column.ID>Column.PropertyText.FilmDailyRoll</Column.ID>
						<Column.Type>48</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="43" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Lab Roll</Column.Name>
						<Column.ID>Column.PropertyText.FilmLabRoll</Column.ID>
						<Column.Type>49</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="44" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Keycode</Column.Name>
						<Column.ID>Column.PropertyText.FilmKeycode</Column.ID>
						<Column.Type>50</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<StringColumn ObjectID="45" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Sync Offset</Column.Name>
						<Column.ID>Column.PropertyText.SyncOffset</Column.ID>
						<Column.Type>44</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="46" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Video Codec</Column.Name>
						<Column.ID>Column.PropertyText.Codec</Column.ID>
						<Column.Type>45</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="47" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Field Order</Column.Name>
						<Column.ID>Column.PropertyText.FieldOrder</Column.ID>
						<Column.Type>46</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="48" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Proxy</Column.Name>
						<Column.ID>Column.PropertyText.Proxy</Column.ID>
						<Column.Type>51</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="49" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Project Locked</Column.Name>
						<Column.ID>Column.PropertyText.BinLocked</Column.ID>
						<Column.Type>52</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<EditTextColumn ObjectID="50" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>ASC_SOP</Column.Name>
						<Column.ID>Column.PropertyText.ASCSOP</Column.ID>
						<Column.Type>53</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="51" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>ASC_SAT</Column.Name>
						<Column.ID>Column.PropertyText.ASCSAT</Column.ID>
						<Column.Type>54</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="52" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Lut</Column.Name>
						<Column.ID>Column.PropertyText.Lut</Column.ID>
						<Column.Type>55</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="53" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Lut1</Column.Name>
						<Column.ID>Column.PropertyText.Lut1</Column.ID>
						<Column.Type>56</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="54" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Lut2</Column.Name>
						<Column.ID>Column.PropertyText.Lut2</Column.ID>
						<Column.Type>57</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="55" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Original Video File Name</Column.Name>
						<Column.ID>Column.PropertyText.OriginalVideoFileName</Column.ID>
						<Column.Type>59</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<EditTextColumn ObjectID="56" ClassID="e9f21f9a-b686-440c-83f4-da1685c160ad" Version="1">
						<Column.Name>Original Audio File Name</Column.Name>
						<Column.ID>Column.PropertyText.OriginalAudioFileName</Column.ID>
						<Column.Type>60</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</EditTextColumn>
					<StringColumn ObjectID="57" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Proxy Media File Path</Column.Name>
						<Column.ID>Column.Intrinsic.ProxyFilePath</Column.ID>
						<Column.Type>37</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="58" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Proxy Media File Name</Column.Name>
						<Column.ID>Column.Intrinsic.ProxyFileName</Column.ID>
						<Column.Type>61</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>true</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="59" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Transcription Status</Column.Name>
						<Column.ID>Column.Intrinsic.TranscriptStatus</Column.ID>
						<Column.Type>63</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</StringColumn>
					<StringColumn ObjectID="60" ClassID="f0ef302d-babc-4f75-9975-923a8ca28d7e" Version="1">
						<Column.Name>Content Credentials</Column.Name>
						<Column.ID>Column.Intrinsic.ContentCredentials</Column.ID>
						<Column.Type>66</Column.Type>
						<Column.Class>0</Column.Class>
						<Column.Width>100</Column.Width>
						<Column.IsHidden>false</Column.IsHidden>
					</StringColumn>
				</ProjectViewState.List>
				<MZ.PrefixKey.OpenSequenceGuidList.1>@SEQUENCE@</MZ.PrefixKey.OpenSequenceGuidList.1>
				<AM.PJShowWellState>0</AM.PJShowWellState>
				<ProjectViewState.Version>2</ProjectViewState.Version>
				<MZ.Project.WorkspaceName>Editing</MZ.Project.WorkspaceName>
				<MZ.BuildVersion.Created>26.3.0x93 - 8/26/2026 6:57:11 PM</MZ.BuildVersion.Created>
				<MZ.BuildVersion.Modified>26.3.0x93 - 10/3/2026 11:52:22 AM</MZ.BuildVersion.Modified>
				<MZ.Project.ApplicationID>Pro</MZ.Project.ApplicationID>
				<MZ.Project.GUID>@PROJECT@</MZ.Project.GUID>
				<TL.PJSnappingState>1</TL.PJSnappingState>
			</Properties>
		</Node>
		<RootProjectItem ObjectURef="@ROOT@"/>
		<ProjectSettings ObjectRef="3"/>
		<MovieCompileSettings ObjectRef="4"/>
		<StillCompileSettings ObjectRef="5"/>
		<AudioCompileSettings ObjectRef="6"/>
		<CustomCompileSettings ObjectRef="7"/>
		<VideoPreviewCompileSettings ObjectRef="8"/>
		<ScratchDiskSettings ObjectRef="9"/>
		<IngestSettings ObjectRef="10"/>
		<ProjectWorkspace ObjectRef="11"/>
		<NextID>1000001</NextID>
	</Project>
)PRPROJ";

static const char PRPROJ_SETTINGS[] = R"PRPROJ(	<ProjectSettings ObjectID="3" ClassID="50c16708-a1a1-4d2f-98d5-4e283ae28353" Version="21">
		<VideoSettings ObjectRef="12"/>
		<AudioSettings ObjectRef="13"/>
		<VideoCompileSettings ObjectRef="14"/>
		<AudioCompileSettings ObjectRef="15"/>
		<CaptureSettings ObjectRef="16"/>
		<DefaultSequenceSettings ObjectRef="17"/>
		<ColorManagementSettings>{"enableLogColorManagement":2,"graphicsWhiteLuminance":203,"lutInterpolationMethod":1}</ColorManagementSettings>
		<VideoTimeDisplay>102</VideoTimeDisplay>
		<AudioTimeDisplay>200</AudioTimeDisplay>
		<ColorAwareEffectsEnabled>0</ColorAwareEffectsEnabled>
		<VideoTimeDisplayInitial>102</VideoTimeDisplayInitial>
		<ActionSafeWidth>10</ActionSafeWidth>
		<ActionSafeHeight>10</ActionSafeHeight>
		<TitleSafeWidth>20</TitleSafeWidth>
		<TitleSafeHeight>20</TitleSafeHeight>
		<ShouldScaleMedia>false</ShouldScaleMedia>
		<EditingModeID>00000000-0000-0000-0000-000000000000</EditingModeID>
		<PreviewFileFormatID>00000000-0000-0000-0000-000000000000</PreviewFileFormatID>
		<UsePreviewCache>false</UsePreviewCache>
	</ProjectSettings>
	<CompileSettings ObjectID="4" ClassID="18a35d66-597e-4157-b783-938b5bec3547" Version="4">
		<VideoCompileSettings ObjectRef="18"/>
		<AudioCompileSettings ObjectRef="19"/>
		<CompilerClassIDFourCC>0</CompilerClassIDFourCC>
		<CompilerFourCC>0</CompilerFourCC>
		<ExportVideo>true</ExportVideo>
		<ExportAudio>true</ExportAudio>
		<AddToProjectWhenFinished>true</AddToProjectWhenFinished>
		<BeepWhenFinished>false</BeepWhenFinished>
		<ExportWorkAreaOnly>false</ExportWorkAreaOnly>
		<EmbedProjectLink>false</EmbedProjectLink>
	</CompileSettings>
	<CompileSettings ObjectID="5" ClassID="18a35d66-597e-4157-b783-938b5bec3547" Version="4">
		<VideoCompileSettings ObjectRef="20"/>
		<AudioCompileSettings ObjectRef="21"/>
		<CompilerClassIDFourCC>0</CompilerClassIDFourCC>
		<CompilerFourCC>0</CompilerFourCC>
		<ExportVideo>true</ExportVideo>
		<ExportAudio>true</ExportAudio>
		<AddToProjectWhenFinished>true</AddToProjectWhenFinished>
		<BeepWhenFinished>false</BeepWhenFinished>
		<ExportWorkAreaOnly>false</ExportWorkAreaOnly>
		<EmbedProjectLink>false</EmbedProjectLink>
	</CompileSettings>
	<CompileSettings ObjectID="6" ClassID="18a35d66-597e-4157-b783-938b5bec3547" Version="4">
		<VideoCompileSettings ObjectRef="22"/>
		<AudioCompileSettings ObjectRef="23"/>
		<CompilerClassIDFourCC>0</CompilerClassIDFourCC>
		<CompilerFourCC>0</CompilerFourCC>
		<ExportVideo>true</ExportVideo>
		<ExportAudio>true</ExportAudio>
		<AddToProjectWhenFinished>true</AddToProjectWhenFinished>
		<BeepWhenFinished>false</BeepWhenFinished>
		<ExportWorkAreaOnly>false</ExportWorkAreaOnly>
		<EmbedProjectLink>false</EmbedProjectLink>
	</CompileSettings>
	<CompileSettings ObjectID="7" ClassID="18a35d66-597e-4157-b783-938b5bec3547" Version="4">
		<VideoCompileSettings ObjectRef="24"/>
		<AudioCompileSettings ObjectRef="25"/>
		<CompilerClassIDFourCC>0</CompilerClassIDFourCC>
		<CompilerFourCC>0</CompilerFourCC>
		<ExportVideo>true</ExportVideo>
		<ExportAudio>true</ExportAudio>
		<AddToProjectWhenFinished>true</AddToProjectWhenFinished>
		<BeepWhenFinished>false</BeepWhenFinished>
		<ExportWorkAreaOnly>false</ExportWorkAreaOnly>
		<EmbedProjectLink>false</EmbedProjectLink>
	</CompileSettings>
	<CompileSettings ObjectID="8" ClassID="18a35d66-597e-4157-b783-938b5bec3547" Version="4">
		<VideoCompileSettings ObjectRef="26"/>
		<AudioCompileSettings ObjectRef="27"/>
		<CompilerClassIDFourCC>1061109567</CompilerClassIDFourCC>
		<CompilerFourCC>1096173910</CompilerFourCC>
		<ExportVideo>true</ExportVideo>
		<ExportAudio>true</ExportAudio>
		<AddToProjectWhenFinished>true</AddToProjectWhenFinished>
		<BeepWhenFinished>false</BeepWhenFinished>
		<ExportWorkAreaOnly>false</ExportWorkAreaOnly>
		<EmbedProjectLink>false</EmbedProjectLink>
	</CompileSettings>
	<ScratchDiskSettings ObjectID="9" ClassID="4c6ed82b-a81c-4df1-8bd0-750504c4b560" Version="4">
		<AudioPreviewLocation0>SameAsProject</AudioPreviewLocation0>
		<VideoPreviewLocation0>SameAsProject</VideoPreviewLocation0>
		<TransferMediaLocation0>SameAsProject</TransferMediaLocation0>
		<CapturedVideoLocation0>SameAsProject</CapturedVideoLocation0>
		<DVDEncodingLocation0>SameAsProject</DVDEncodingLocation0>
		<AutoSaveLocation0>SameAsProject</AutoSaveLocation0>
		<CapsuleMediaLocation0>SameAsProject</CapsuleMediaLocation0>
		<CCLibrariesLocation0>SameAsProject</CCLibrariesLocation0>
	</ScratchDiskSettings>
	<IngestSettings ObjectID="10" ClassID="2db8f76b-2c37-48ee-925d-9a4f7278152d" Version="2">
		<Action>copy</Action>
		<Enabled>false</Enabled>
	</IngestSettings>
	<WorkspaceSettings ObjectID="11" ClassID="c4372273-e1aa-4683-98aa-a2ceadf3066c" Version="1">
	</WorkspaceSettings>
	<VideoSettings ObjectID="12" ClassID="58474264-30c4-43a2-bba5-dc0812df8a3a" Version="10">
	</VideoSettings>
	<AudioSettings ObjectID="13" ClassID="6baf5521-b132-4634-840e-13cec5bc86a4" Version="8">
	</AudioSettings>
	<VideoCompileSettings ObjectID="14" ClassID="db372db5-7de2-4d3c-98ae-f42659d77b22" Version="9">
		<VideoSettings ObjectRef="28"/>
		<VideoFieldType>0</VideoFieldType>
		<Compressor>1685288558</Compressor>
		<VideoCompilerClassIDFourCC>1061109567</VideoCompilerClassIDFourCC>
		<VideoFileTypeFourCC>1096173910</VideoFileTypeFourCC>
		<Depth>24</Depth>
		<RenderDepth>0</RenderDepth>
		<Aspect43>false</Aspect43>
		<Quality>100</Quality>
		<UseDataRate>false</UseDataRate>
		<DataRate>3500</DataRate>
		<ForceRecompress>true</ForceRecompress>
		<ForceRecompressValue>2</ForceRecompressValue>
		<Deinterlace>false</Deinterlace>
		<IgnoreVideoFilters>false</IgnoreVideoFilters>
		<OptimizeStills>false</OptimizeStills>
		<FramesAtMarkers>false</FramesAtMarkers>
		<RealTimePreview>true</RealTimePreview>
		<DoKeyframeEveryNFrames>false</DoKeyframeEveryNFrames>
		<DoKeyframeEveryNFramesValue>0</DoKeyframeEveryNFramesValue>
		<AddKeyframesAtMarkers>false</AddKeyframesAtMarkers>
		<AddKeyframesAtEdits>false</AddKeyframesAtEdits>
		<RelativeFrameSize>1</RelativeFrameSize>
	</VideoCompileSettings>
	<AudioCompileSettings ObjectID="15" ClassID="34b10007-ab6d-49a7-bac5-7b60d919e387" Version="6">
		<AudioSettings ObjectRef="29"/>
		<Interleave>1</Interleave>
		<SampleType>3</SampleType>
		<Compressor>1380013856</Compressor>
	</AudioCompileSettings>
	<DummyCaptureSettings ObjectID="16" ClassID="328c2aa2-47f9-4211-805b-b6a6dbd4ca29" Version="1">
	</DummyCaptureSettings>
	<DefaultSequenceSettings ObjectID="17" ClassID="567bdf53-d6d9-4d61-b2f1-f4834bebea9b" Version="2">
		<TotalVideoTracks>1</TotalVideoTracks>
		<DefaultAudioStandardMonoTracks>0</DefaultAudioStandardMonoTracks>
		<DefaultAudioStandardStereoTracks>1</DefaultAudioStandardStereoTracks>
		<DefaultAudioStandard51Tracks>0</DefaultAudioStandard51Tracks>
		<DefaultAudioSubmixMonoTracks>0</DefaultAudioSubmixMonoTracks>
		<DefaultAudioSubmixStereoTracks>0</DefaultAudioSubmixStereoTracks>
		<DefaultAudioSubmix51Tracks>0</DefaultAudioSubmix51Tracks>
	</DefaultSequenceSettings>
	<VideoCompileSettings ObjectID="18" ClassID="db372db5-7de2-4d3c-98ae-f42659d77b22" Version="9">
		<VideoSettings ObjectRef="30"/>
		<VideoFieldType>0</VideoFieldType>
		<Compressor>1685288558</Compressor>
		<VideoCompilerClassIDFourCC>1061109567</VideoCompilerClassIDFourCC>
		<VideoFileTypeFourCC>1096173910</VideoFileTypeFourCC>
		<Depth>24</Depth>
		<RenderDepth>0</RenderDepth>
		<Aspect43>false</Aspect43>
		<Quality>100</Quality>
		<UseDataRate>false</UseDataRate>
		<DataRate>3500</DataRate>
		<ForceRecompress>true</ForceRecompress>
		<ForceRecompressValue>2</ForceRecompressValue>
		<Deinterlace>false</Deinterlace>
		<IgnoreVideoFilters>false</IgnoreVideoFilters>
		<OptimizeStills>false</OptimizeStills>
		<FramesAtMarkers>false</FramesAtMarkers>
		<RealTimePreview>true</RealTimePreview>
		<DoKeyframeEveryNFrames>false</DoKeyframeEveryNFrames>
		<DoKeyframeEveryNFramesValue>0</DoKeyframeEveryNFramesValue>
		<AddKeyframesAtMarkers>false</AddKeyframesAtMarkers>
		<AddKeyframesAtEdits>false</AddKeyframesAtEdits>
		<RelativeFrameSize>1</RelativeFrameSize>
	</VideoCompileSettings>
	<AudioCompileSettings ObjectID="19" ClassID="34b10007-ab6d-49a7-bac5-7b60d919e387" Version="6">
		<AudioSettings ObjectRef="31"/>
		<Interleave>1</Interleave>
		<SampleType>3</SampleType>
		<Compressor>1380013856</Compressor>
	</AudioCompileSettings>
	<VideoCompileSettings ObjectID="20" ClassID="db372db5-7de2-4d3c-98ae-f42659d77b22" Version="9">
		<VideoSettings ObjectRef="32"/>
		<VideoFieldType>0</VideoFieldType>
		<Compressor>1685288558</Compressor>
		<VideoCompilerClassIDFourCC>1061109567</VideoCompilerClassIDFourCC>
		<VideoFileTypeFourCC>1096173910</VideoFileTypeFourCC>
		<Depth>24</Depth>
		<RenderDepth>0</RenderDepth>
		<Aspect43>false</Aspect43>
		<Quality>100</Quality>
		<UseDataRate>false</UseDataRate>
		<DataRate>3500</DataRate>
		<ForceRecompress>true</ForceRecompress>
		<ForceRecompressValue>2</ForceRecompressValue>
		<Deinterlace>false</Deinterlace>
		<IgnoreVideoFilters>false</IgnoreVideoFilters>
		<OptimizeStills>false</OptimizeStills>
		<FramesAtMarkers>false</FramesAtMarkers>
		<RealTimePreview>true</RealTimePreview>
		<DoKeyframeEveryNFrames>false</DoKeyframeEveryNFrames>
		<DoKeyframeEveryNFramesValue>0</DoKeyframeEveryNFramesValue>
		<AddKeyframesAtMarkers>false</AddKeyframesAtMarkers>
		<AddKeyframesAtEdits>false</AddKeyframesAtEdits>
		<RelativeFrameSize>1</RelativeFrameSize>
	</VideoCompileSettings>
	<AudioCompileSettings ObjectID="21" ClassID="34b10007-ab6d-49a7-bac5-7b60d919e387" Version="6">
		<AudioSettings ObjectRef="33"/>
		<Interleave>1</Interleave>
		<SampleType>3</SampleType>
		<Compressor>1380013856</Compressor>
	</AudioCompileSettings>
	<VideoCompileSettings ObjectID="22" ClassID="db372db5-7de2-4d3c-98ae-f42659d77b22" Version="9">
		<VideoSettings ObjectRef="34"/>
		<VideoFieldType>0</VideoFieldType>
		<Compressor>1685288558</Compressor>
		<VideoCompilerClassIDFourCC>1061109567</VideoCompilerClassIDFourCC>
		<VideoFileTypeFourCC>1096173910</VideoFileTypeFourCC>
		<Depth>24</Depth>
		<RenderDepth>0</RenderDepth>
		<Aspect43>false</Aspect43>
		<Quality>100</Quality>
		<UseDataRate>false</UseDataRate>
		<DataRate>3500</DataRate>
		<ForceRecompress>true</ForceRecompress>
		<ForceRecompressValue>2</ForceRecompressValue>
		<Deinterlace>false</Deinterlace>
		<IgnoreVideoFilters>false</IgnoreVideoFilters>
		<OptimizeStills>false</OptimizeStills>
		<FramesAtMarkers>false</FramesAtMarkers>
		<RealTimePreview>true</RealTimePreview>
		<DoKeyframeEveryNFrames>false</DoKeyframeEveryNFrames>
		<DoKeyframeEveryNFramesValue>0</DoKeyframeEveryNFramesValue>
		<AddKeyframesAtMarkers>false</AddKeyframesAtMarkers>
		<AddKeyframesAtEdits>false</AddKeyframesAtEdits>
		<RelativeFrameSize>1</RelativeFrameSize>
	</VideoCompileSettings>
	<AudioCompileSettings ObjectID="23" ClassID="34b10007-ab6d-49a7-bac5-7b60d919e387" Version="6">
		<AudioSettings ObjectRef="35"/>
		<Interleave>1</Interleave>
		<SampleType>3</SampleType>
		<Compressor>1380013856</Compressor>
	</AudioCompileSettings>
	<VideoCompileSettings ObjectID="24" ClassID="db372db5-7de2-4d3c-98ae-f42659d77b22" Version="9">
		<VideoSettings ObjectRef="36"/>
		<VideoFieldType>0</VideoFieldType>
		<Compressor>1685288558</Compressor>
		<VideoCompilerClassIDFourCC>1061109567</VideoCompilerClassIDFourCC>
		<VideoFileTypeFourCC>1096173910</VideoFileTypeFourCC>
		<Depth>24</Depth>
		<RenderDepth>0</RenderDepth>
		<Aspect43>false</Aspect43>
		<Quality>100</Quality>
		<UseDataRate>false</UseDataRate>
		<DataRate>3500</DataRate>
		<ForceRecompress>true</ForceRecompress>
		<ForceRecompressValue>2</ForceRecompressValue>
		<Deinterlace>false</Deinterlace>
		<IgnoreVideoFilters>false</IgnoreVideoFilters>
		<OptimizeStills>false</OptimizeStills>
		<FramesAtMarkers>false</FramesAtMarkers>
		<RealTimePreview>true</RealTimePreview>
		<DoKeyframeEveryNFrames>false</DoKeyframeEveryNFrames>
		<DoKeyframeEveryNFramesValue>0</DoKeyframeEveryNFramesValue>
		<AddKeyframesAtMarkers>false</AddKeyframesAtMarkers>
		<AddKeyframesAtEdits>false</AddKeyframesAtEdits>
		<RelativeFrameSize>1</RelativeFrameSize>
	</VideoCompileSettings>
	<AudioCompileSettings ObjectID="25" ClassID="34b10007-ab6d-49a7-bac5-7b60d919e387" Version="6">
		<AudioSettings ObjectRef="37"/>
		<Interleave>1</Interleave>
		<SampleType>3</SampleType>
		<Compressor>1380013856</Compressor>
	</AudioCompileSettings>
	<VideoCompileSettings ObjectID="26" ClassID="db372db5-7de2-4d3c-98ae-f42659d77b22" Version="9">
		<VideoSettings ObjectRef="38"/>
		<VideoFieldType>0</VideoFieldType>
		<Compressor>1685288558</Compressor>
		<VideoCompilerClassIDFourCC>1061109567</VideoCompilerClassIDFourCC>
		<VideoFileTypeFourCC>1096173910</VideoFileTypeFourCC>
		<Depth>24</Depth>
		<RenderDepth>0</RenderDepth>
		<Aspect43>false</Aspect43>
		<Quality>100</Quality>
		<UseDataRate>false</UseDataRate>
		<DataRate>3500</DataRate>
		<ForceRecompress>true</ForceRecompress>
		<ForceRecompressValue>2</ForceRecompressValue>
		<Deinterlace>false</Deinterlace>
		<IgnoreVideoFilters>false</IgnoreVideoFilters>
		<OptimizeStills>false</OptimizeStills>
		<FramesAtMarkers>false</FramesAtMarkers>
		<RealTimePreview>true</RealTimePreview>
		<DoKeyframeEveryNFrames>false</DoKeyframeEveryNFrames>
		<DoKeyframeEveryNFramesValue>0</DoKeyframeEveryNFramesValue>
		<AddKeyframesAtMarkers>false</AddKeyframesAtMarkers>
		<AddKeyframesAtEdits>false</AddKeyframesAtEdits>
		<RelativeFrameSize>1</RelativeFrameSize>
	</VideoCompileSettings>
	<AudioCompileSettings ObjectID="27" ClassID="34b10007-ab6d-49a7-bac5-7b60d919e387" Version="6">
		<AudioSettings ObjectRef="39"/>
		<Interleave>1</Interleave>
		<SampleType>3</SampleType>
		<Compressor>1380013856</Compressor>
	</AudioCompileSettings>
	<VideoSettings ObjectID="28" ClassID="58474264-30c4-43a2-bba5-dc0812df8a3a" Version="10">
	</VideoSettings>
	<AudioSettings ObjectID="29" ClassID="6baf5521-b132-4634-840e-13cec5bc86a4" Version="8">
	</AudioSettings>
	<VideoSettings ObjectID="30" ClassID="58474264-30c4-43a2-bba5-dc0812df8a3a" Version="10">
	</VideoSettings>
	<AudioSettings ObjectID="31" ClassID="6baf5521-b132-4634-840e-13cec5bc86a4" Version="8">
	</AudioSettings>
	<VideoSettings ObjectID="32" ClassID="58474264-30c4-43a2-bba5-dc0812df8a3a" Version="10">
	</VideoSettings>
	<AudioSettings ObjectID="33" ClassID="6baf5521-b132-4634-840e-13cec5bc86a4" Version="8">
	</AudioSettings>
	<VideoSettings ObjectID="34" ClassID="58474264-30c4-43a2-bba5-dc0812df8a3a" Version="10">
	</VideoSettings>
	<AudioSettings ObjectID="35" ClassID="6baf5521-b132-4634-840e-13cec5bc86a4" Version="8">
	</AudioSettings>
	<VideoSettings ObjectID="36" ClassID="58474264-30c4-43a2-bba5-dc0812df8a3a" Version="10">
	</VideoSettings>
	<AudioSettings ObjectID="37" ClassID="6baf5521-b132-4634-840e-13cec5bc86a4" Version="8">
	</AudioSettings>
	<VideoSettings ObjectID="38" ClassID="58474264-30c4-43a2-bba5-dc0812df8a3a" Version="10">
	</VideoSettings>
	<AudioSettings ObjectID="39" ClassID="6baf5521-b132-4634-840e-13cec5bc86a4" Version="8">
	</AudioSettings>
)PRPROJ";

// the first ObjectID that is not in the parts above
static const unsigned int PRPROJ_FIRST_ID = 40;

#endif //MCM_PRPROJ_TEMPLATE_H
